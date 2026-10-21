#include "Quests/ASQuestComponent.h"

#include "Characters/Player/Components/ASProgressionComponent.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Characters/Player/Data/ASProgressionData.h"
#include "Core/Framework/ASPlayerState.h"
#include "Modules/Persistence/ASPlayerProfileComponent.h"
#include "Net/UnrealNetwork.h"
#include "Quests/Data/ASQuestDefinition.h"

UASQuestComponent::UASQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
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
	if (!HasQuestAuthority() || !bPersistenceReady || !IsValid(Definition) || Quests.Num() >= ASQuestLimits::MaxQuests
		|| Definition->Objectives.Num() > ASQuestLimits::MaxObjectives)
	{
		return false;
	}
	// Reject configuration mistakes now instead of writing a quest that cannot load next run.
	if (!QuestCatalog.Contains(Definition) && !StartingQuests.Contains(Definition)) return false;

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

bool UASQuestComponent::HandleQuestEvent(const FASQuestEvent& Event)
{
	if (!HasQuestAuthority() || !bPersistenceReady || !Event.EventTag.IsValid() || Event.Amount <= 0)
	{
		return false;
	}

	bool bChanged = false;
	for (FASQuestProgress& Quest : Quests)
	{
		if (Quest.Status != EASQuestStatus::InProgress || !IsValid(Quest.Definition)
			|| Quest.Objectives.Num() != Quest.Definition->Objectives.Num())
		{
			continue;
		}

		bool bAllComplete = true;
		for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
		{
			FASQuestObjectiveProgress& Progress = Quest.Objectives[Index];
			const FASQuestObjectiveDefinition& Objective = Quest.Definition->Objectives[Index];
			if (Progress.CurrentCount < Objective.RequiredCount && Event.EventTag.MatchesTag(Objective.EventTag)
				&& (!Objective.TargetTag.IsValid() || Event.TargetTag.MatchesTag(Objective.TargetTag)))
			{
				// Limit the delta before addition so a large batch cannot overflow int32.
				Progress.CurrentCount += FMath::Min(Event.Amount, Objective.RequiredCount - Progress.CurrentCount);
				bChanged = true;
			}
			bAllComplete &= Progress.CurrentCount >= Objective.RequiredCount;
		}
		if (bAllComplete)
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
	}
	return Data;
}

bool UASQuestComponent::RestoreSaveData(const FASQuestSaveData& Data, FString& OutError)
{
	OutError.Reset();
	if (!HasQuestAuthority())
	{
		OutError = TEXT("Only the server can restore quest progress.");
		return false;
	}
	if (!Data.Validate(OutError)) return false;

	TMap<FName, UASQuestDefinition*> Definitions;
	const auto RegisterDefinition = [&Definitions, &OutError](UASQuestDefinition* Definition)
	{
		if (!IsValid(Definition) || !Definition->ValidateDefinition(OutError))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Quest catalog contains a missing asset.");
			return false;
		}
		if (UASQuestDefinition** Existing = Definitions.Find(Definition->QuestId); Existing && *Existing != Definition)
		{
			OutError = FString::Printf(TEXT("Duplicate QuestId in catalog: %s"), *Definition->QuestId.ToString());
			return false;
		}
		Definitions.Add(Definition->QuestId, Definition);
		return true;
	};
	for (UASQuestDefinition* Definition : QuestCatalog) if (!RegisterDefinition(Definition)) return false;
	for (UASQuestDefinition* Definition : StartingQuests) if (!RegisterDefinition(Definition)) return false;

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
		bool bAllComplete = true;
		for (const FASQuestObjectiveDefinition& Objective : (*Definition)->Objectives)
		{
			const FASQuestObjectiveProgress* Previous = Saved.Objectives.FindByPredicate(
				[&Objective](const FASQuestObjectiveProgress& Entry) { return Entry.ObjectiveId == Objective.ObjectiveId; });
			FASQuestObjectiveProgress& Current = Progress.Objectives.AddDefaulted_GetRef();
			Current.ObjectiveId = Objective.ObjectiveId;
			Current.CurrentCount = Previous ? FMath::Min(Previous->CurrentCount, Objective.RequiredCount) : 0;
			bAllComplete &= Current.CurrentCount >= Objective.RequiredCount;
		}
		// A design update must never make a previously claimed reward claimable again.
		Progress.Status = Saved.Status == EASQuestStatus::RewardClaimed ? EASQuestStatus::RewardClaimed
			: (bAllComplete ? EASQuestStatus::ReadyToTurnIn : EASQuestStatus::InProgress);
		Restored.Add(MoveTemp(Progress));
	}
	for (UASQuestDefinition* Definition : StartingQuests)
	{
		if (Restored.ContainsByPredicate([Definition](const FASQuestProgress& Quest) { return Quest.QuestId == Definition->QuestId; })) continue;
		FASQuestProgress& Progress = Restored.AddDefaulted_GetRef();
		Progress.QuestId = Definition->QuestId;
		Progress.Definition = Definition;
		for (const FASQuestObjectiveDefinition& Objective : Definition->Objectives)
		{
			FASQuestObjectiveProgress& Current = Progress.Objectives.AddDefaulted_GetRef();
			Current.ObjectiveId = Objective.ObjectiveId;
		}
	}
	if (Restored.Num() > ASQuestLimits::MaxQuests)
	{
		OutError = TEXT("Restored and starting quests exceed the maximum of 256 quests.");
		return false;
	}
	Quests = MoveTemp(Restored);
	bPersistenceReady = true;
	NotifyQuestsChanged();
	return true;
}

void UASQuestComponent::OnRep_Quests()
{
	OnQuestsChanged.Broadcast();
}
