#include "Quests/ASQuestComponent.h"

#include "Characters/Player/Components/ASProgressionComponent.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Characters/Player/Data/ASProgressionData.h"
#include "Core/Framework/ASPlayerState.h"
#include "Modules/Persistence/ASPlayerProfileComponent.h"
#include "Net/UnrealNetwork.h"
#include "Quests/Data/ASQuestDefinition.h"
#include "Quests/Data/ASQuestCatalog.h"
#include "Quests/Data/ASQuestSettings.h"
#include "Quests/Data/ASQuestTags.h"

UASQuestComponent::UASQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;
}

void UASQuestComponent::InitializeComponent()
{
	Super::InitializeComponent();
	QuestCatalog.Reset();
	CatalogLoadError.Reset();
	const TSoftObjectPtr<UASQuestCatalog>& CatalogAsset = GetDefault<UASQuestSettings>()->Catalog;
	if (!CatalogAsset.IsNull())
	{
		if (const UASQuestCatalog* Catalog = CatalogAsset.LoadSynchronous())
		{
			if (Catalog->ValidateCatalog(CatalogLoadError))
				for (UASQuestDefinition* Definition : Catalog->Quests) QuestCatalog.AddUnique(Definition);
		}
		else CatalogLoadError = FString::Printf(TEXT("Could not load quest catalog %s."), *CatalogAsset.ToString());
	}
	else CatalogLoadError = TEXT("Set Project Settings -> Quests -> Catalog before loading a profile.");
}

void UASQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UASQuestComponent, Quests, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UASQuestComponent, bPersistenceReady, COND_OwnerOnly);
}

void UASQuestComponent::BeginPlay()
{
	Super::BeginPlay();
	// A standalone quest actor can initialize itself; PlayerState uses its profile bridge.
	if (HasQuestAuthority() && !GetOwner()->FindComponentByClass<UASPlayerProfileComponent>() && !bPersistenceReady)
	{
		FString Error;
		if (!RestoreSaveData(FASQuestSaveData(), Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Quest] Could not initialize starting quests: %s"), *Error);
		}
	}
}

bool UASQuestComponent::HasQuestAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

bool UASQuestComponent::IsPersistenceReady() const
{
	return bPersistenceReady;
}

bool UASQuestComponent::AcceptQuest(UASQuestDefinition* Definition)
{
	FScopedQuestChange Change(*this);
	if (!HasQuestAuthority() || !CanAcceptQuest(Definition) || Quests.Num() >= ASQuestLimits::MaxQuests
		|| Definition->Objectives.Num() > ASQuestLimits::MaxObjectives)
	{
		return false;
	}
	FString Error;
	if (!Definition->ValidateDefinition(Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Quest] Cannot accept %s: %s"), *GetNameSafe(Definition), *Error);
		return false;
	}
	if (Quests.ContainsByPredicate([Definition](const FASQuestProgress& Quest)
		{ return Quest.QuestId == Definition->QuestId; }))
	{
		return false;
	}

	FASQuestProgress Progress;
	Progress.QuestId = Definition->QuestId;
	Progress.Definition = Definition;
	for (const FASQuestObjectiveDefinition& Objective : Definition->Objectives)
	{
		FASQuestObjectiveProgress& ObjectiveProgress = Progress.Objectives.AddDefaulted_GetRef();
		ObjectiveProgress.ObjectiveId = Objective.ObjectiveId;
	}
	Quests.Add(MoveTemp(Progress));
	NotifyQuestsChanged();
	return true;
}

bool UASQuestComponent::CanAcceptQuest(const UASQuestDefinition* Definition) const
{
	if (!bPersistenceReady || !IsValid(Definition)
		|| !QuestCatalog.Contains(Definition)) return false;
	FASQuestProgress Existing;
	if (GetQuestProgress(Definition->QuestId, Existing)) return false;
	for (FName Required : Definition->PrerequisiteQuestIds)
	{
		if (!GetQuestProgress(Required, Existing) || Existing.Status != EASQuestStatus::RewardClaimed) return false;
	}
	return true;
}

bool UASQuestComponent::HandleQuestEvent(const FASQuestEvent& Event)
{
	return ApplyQuestEvent(Event, nullptr);
}

bool UASQuestComponent::ApplyQuestEvent(const FASQuestEvent& Event, const UASItemDefinition* CollectedItem)
{
	FScopedQuestChange Change(*this);
	if (!HasQuestAuthority() || !bPersistenceReady || !Event.EventTag.IsValid() || Event.Amount <= 0)
	{
		return false;
	}

	bool bChanged = false;
	for (FASQuestProgress& Quest : Quests)
	{
		if (Quest.Status != EASQuestStatus::InProgress || !Quest.HasValidObjectives())
		{
			continue;
		}

		// Compute eligibility before mutation so one event cannot unlock and advance the next step.
		for (int32 Index : Quest.GetEligibleObjectiveIndices())
		{
			FASQuestObjectiveProgress& Progress = Quest.Objectives[Index];
			const FASQuestObjectiveDefinition& Objective = Quest.Definition->Objectives[Index];
			if (ASQuestRules::MatchesObjective(Objective, Event, CollectedItem, GetOwner()))
			{
				// Limit the delta before addition so a large batch cannot overflow int32.
				Progress.CurrentCount += FMath::Min(Event.Amount, Objective.RequiredCount - Progress.CurrentCount);
				bChanged = true;
			}
		}
		// Completion is a run-clear receipt, never just a consequence of filling objective counters.
		// An earlier boss kill cannot be banked and consumed by a later objective event.
		if (Quest.AreObjectivesComplete() && Event.EventTag == ASGameplayTags::Quest_Event_FinalBossDefeated)
		{
			Quest.Status = EASQuestStatus::ReadyToTurnIn;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		// Broadcast after iteration; UI/gameplay listeners may accept another quest.
		NotifyQuestsChanged();
	}
	return bChanged;
}

bool UASQuestComponent::ClaimReward(FName QuestId)
{
	return ClaimRewardInternal(QuestId, NAME_None);
}

bool UASQuestComponent::ClaimRewardInternal(FName QuestId, FName NpcId)
{
	FScopedQuestChange Change(*this);
	if (!HasQuestAuthority() || !bPersistenceReady)
	{
		return false;
	}
	FASQuestProgress* Quest = Quests.FindByPredicate([QuestId](const FASQuestProgress& Progress)
		{ return Progress.QuestId == QuestId; });
	if (!Quest || Quest->Status != EASQuestStatus::ReadyToTurnIn || !IsValid(Quest->Definition))
	{
		return false;
	}

	if (Quest->Definition->TurnInNpcId != NpcId) return false;

	const int32 Experience = Quest->Definition->ExperienceReward;
	UASProgressionComponent* Progression = nullptr;
	if (Experience > 0)
	{
		const AASPlayerState* PlayerState = Cast<AASPlayerState>(GetOwner());
		Progression = PlayerState ? PlayerState->GetProgressionComponent() : nullptr;
		const UASCharacterDefinition* Character = PlayerState ? PlayerState->GetCharacterDefinition() : nullptr;
		const UASProgressionData* Data = Character ? Character->ProgressionData.Get() : nullptr;
		if (!Progression || !Data || (!Progression->IsMaxLevel() && Data->ExpToNextLevel.IsEmpty()))
		{
			return false;
		}
		// AddExperience uses int32 accumulation. Do not consume a reward it cannot safely add.
		if (!Progression->IsMaxLevel() && Experience > MAX_int32 - Progression->GetCurrentExp())
		{
			return false;
		}
	}

	// Mark before granting: AddExperience broadcasts and can cause reentrant claim requests.
	Quest->Status = EASQuestStatus::RewardClaimed;
	if (Progression)
	{
		// Existing progression policy discards experience at max level.
		Progression->AddExperience(Experience);
	}
	// Reporting unlocks the next offer; accepting it is a separate NPC interaction.
	NotifyQuestsChanged();
	return true;
}

void UASQuestComponent::ServerClaimReward_Implementation(FName QuestId)
{
	ClaimReward(QuestId);
}

bool UASQuestComponent::GetQuestProgress(FName QuestId, FASQuestProgress& OutProgress) const
{
	const FASQuestProgress* Quest = Quests.FindByPredicate([QuestId](const FASQuestProgress& Progress)
		{ return Progress.QuestId == QuestId; });
	OutProgress = Quest ? *Quest : FASQuestProgress();
	return Quest != nullptr;
}

void UASQuestComponent::NotifyQuestsChanged()
{
	bQuestChangePending = true;
	if (QuestChangeDepth == 0) FlushQuestChanges();
}

UASQuestComponent::FScopedQuestChange::FScopedQuestChange(UASQuestComponent& InOwner) : Owner(InOwner)
{
	++Owner.QuestChangeDepth;
}

UASQuestComponent::FScopedQuestChange::~FScopedQuestChange()
{
	if (--Owner.QuestChangeDepth == 0) Owner.FlushQuestChanges();
}

void UASQuestComponent::FlushQuestChanges()
{
	if (!bQuestChangePending) return;
	bQuestChangePending = false;
	GetOwner()->ForceNetUpdate();
	OnQuestsChanged.Broadcast();
}

FASQuestSaveData UASQuestComponent::ExportSaveData() const
{
	FASQuestSaveData Data;
	for (const FASQuestProgress& Quest : Quests)
	{
		FASQuestSaveRecord& Record = Data.Quests.AddDefaulted_GetRef();
		Record.QuestId = Quest.QuestId;
		Record.Status = Quest.Status;
		Record.Objectives = Quest.Objectives;
		if (Quest.Definition && Quest.Definition->bRunBoundObjectives && Quest.Status == EASQuestStatus::InProgress)
		{
			for (FASQuestObjectiveProgress& Objective : Record.Objectives) Objective.CurrentCount = 0;
		}
	}
	return Data;
}

bool UASQuestComponent::RestoreSaveData(const FASQuestSaveData& Data, FString& OutError)
{
	FScopedQuestChange Change(*this);
	OutError.Reset();
	if (!HasQuestAuthority())
	{
		OutError = TEXT("Only the server can restore quest progress.");
		return false;
	}
	if (!Data.Validate(OutError)) return false;
	if (!CatalogLoadError.IsEmpty()) { OutError = CatalogLoadError; return false; }
	if (!UASQuestCatalog::ValidateDefinitions(QuestCatalog, OutError)) return false;

	TMap<FName, UASQuestDefinition*> Definitions;
	for (UASQuestDefinition* Definition : QuestCatalog) Definitions.Add(Definition->QuestId, Definition);

	TArray<FASQuestProgress> Restored;
	for (const FASQuestSaveRecord& Saved : Data.Quests)
	{
		UASQuestDefinition* const* Definition = Definitions.Find(Saved.QuestId);
		if (!Definition)
		{
			OutError = FString::Printf(TEXT("Register saved quest %s in QuestCatalog before loading this profile."), *Saved.QuestId.ToString());
			return false;
		}
		FASQuestProgress Progress;
		Progress.QuestId = Saved.QuestId;
		Progress.Definition = *Definition;
		for (const FASQuestObjectiveDefinition& Objective : (*Definition)->Objectives)
		{
			const FASQuestObjectiveProgress* Previous = Saved.Objectives.FindByPredicate(
				[&Objective](const FASQuestObjectiveProgress& Entry) { return Entry.ObjectiveId == Objective.ObjectiveId; });
			FASQuestObjectiveProgress& Current = Progress.Objectives.AddDefaulted_GetRef();
			Current.ObjectiveId = Objective.ObjectiveId;
			Current.CurrentCount = Previous ? FMath::Min(Previous->CurrentCount, Objective.RequiredCount) : 0;
		}
		// A design update must never make a previously claimed reward claimable again.
		Progress.Status = Saved.Status == EASQuestStatus::RewardClaimed ? EASQuestStatus::RewardClaimed
			: (Saved.Status == EASQuestStatus::ReadyToTurnIn && Progress.AreObjectivesComplete()
				? EASQuestStatus::ReadyToTurnIn : EASQuestStatus::InProgress);
		Restored.Add(MoveTemp(Progress));
	}
	if (Restored.Num() > ASQuestLimits::MaxQuests)
	{
		OutError = TEXT("Restored quests exceed the maximum of 256 quests.");
		return false;
	}
	Quests = MoveTemp(Restored);
	bPersistenceReady = true;
	AcceptAvailableAutomaticQuests();
	NotifyQuestsChanged();
	return true;
}

bool UASQuestComponent::IsAutomaticQuest(const UASQuestDefinition* Definition) const
{
	return Definition && Definition->PrerequisiteQuestIds.IsEmpty() && Definition->bAutoAccept;
}

void UASQuestComponent::AcceptAvailableAutomaticQuests()
{
	for (UASQuestDefinition* Definition : QuestCatalog)
		if (IsAutomaticQuest(Definition) && CanAcceptQuest(Definition))
			AcceptQuest(Definition);
}

void UASQuestComponent::OnRep_Quests()
{
	OnQuestsChanged.Broadcast();
}

void UASQuestComponent::EndQuestRun()
{
	FScopedQuestChange Change(*this);
	if (!HasQuestAuthority()) return;
	bool bChanged = false;
	for (FASQuestProgress& Quest : Quests)
	{
		if (!Quest.Definition || !Quest.Definition->bRunBoundObjectives || Quest.Status != EASQuestStatus::InProgress) continue;
		for (FASQuestObjectiveProgress& Objective : Quest.Objectives)
		{
			bChanged |= Objective.CurrentCount != 0;
			Objective.CurrentCount = 0;
		}
	}
	if (bChanged) NotifyQuestsChanged();
}
