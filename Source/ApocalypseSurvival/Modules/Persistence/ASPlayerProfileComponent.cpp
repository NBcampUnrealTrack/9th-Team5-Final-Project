#include "Modules/Persistence/ASPlayerProfileComponent.h"

#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Modules/Data/ASModuleSettings.h"
#include "Modules/Persistence/ASModuleCollectionSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Quests/ASQuestComponent.h"
#include "TimerManager.h"

UASPlayerProfileComponent::UASPlayerProfileComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UASPlayerProfileComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UASPlayerProfileComponent, bProfileInitialized, COND_OwnerOnly);
}

UASQuestComponent* UASPlayerProfileComponent::GetQuests() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UASQuestComponent>() : nullptr;
}

void UASPlayerProfileComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		if (UASQuestComponent* Quests = GetQuests())
		{
			Quests->OnQuestsChanged.AddDynamic(this, &ThisClass::HandleQuestsChanged);
		}
		// The owning controller/connection may not yet be available at PlayerState BeginPlay.
		GetWorld()->GetTimerManager().SetTimer(LoadRetryTimer, this, &ThisClass::TryLoadOwnerProfile, 0.5f, true);
		TryLoadOwnerProfile();
	}
}

void UASPlayerProfileComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(LoadRetryTimer);
	if (UASQuestComponent* Quests = GetQuests())
	{
		Quests->OnQuestsChanged.RemoveDynamic(this, &ThisClass::HandleQuestsChanged);
	}
	if (LocalCollection.IsValid())
	{
		LocalCollection->OnCollectionChanged.RemoveDynamic(this, &ThisClass::HandleLocalModulesChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UASPlayerProfileComponent::TryLoadOwnerProfile()
{
	if (bProfileInitialized || bInitializationFailed)
	{
		GetWorld()->GetTimerManager().ClearTimer(LoadRetryTimer);
		return;
	}
	const APlayerState* PlayerState = Cast<APlayerState>(GetOwner());
	const APlayerController* Controller = PlayerState ? PlayerState->GetPlayerController() : nullptr;
	if (Controller && (Controller->IsLocalController() || Controller->GetNetConnection()))
	{
		ClientLoadLocalProfile();
	}
}

void UASPlayerProfileComponent::ClientLoadLocalProfile_Implementation()
{
	if (bLocalProfileSubmitted) return;
	const APlayerState* PlayerState = Cast<APlayerState>(GetOwner());
	const APlayerController* Controller = PlayerState ? PlayerState->GetPlayerController() : nullptr;
	UASModuleCollectionSubsystem* Collection = UASModuleCollectionSubsystem::Get(Controller);
	if (!Collection) return; // A later server retry handles local-player setup still in progress.
	FString Error;
	if (!Collection->IsProfileLoaded() && !Collection->LoadProfile(Error))
	{
		ReportLocalProfileError(Error);
		ServerReportLoadFailure(Error);
		return;
	}
	LocalCollection = Collection;
	Collection->OnCollectionChanged.AddUniqueDynamic(this, &ThisClass::HandleLocalModulesChanged);
	bLocalProfileSubmitted = true;
	ServerRestoreLocalProfile(Collection->GetSavedQuestProgress(), Collection->GetUnlockedModuleIds());
}

bool UASPlayerProfileComponent::ResolveUnlockedModules(const TArray<FName>& Ids, TArray<FName>& OutIds, FString& OutError) const
{
	OutError.Reset();
	OutIds.Reset();
	if (Ids.Num() > 4096)
	{
		OutError = TEXT("The personal module unlock list exceeds 4096 entries.");
		return false;
	}
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	for (FName Id : Ids)
	{
		FString LookupError;
		// Retired or currently unconfigured modules stay in the save, but cannot drop.
		if (!Id.IsNone() && Settings->FindModule(Id, LookupError)) OutIds.AddUnique(Id);
	}
	return true;
}

void UASPlayerProfileComponent::ServerRestoreLocalProfile_Implementation(
	const FASQuestSaveData& Quests, const TArray<FName>& UnlockedModules)
{
	if (bProfileInitialized || bInitializationFailed) return;
	FString Error;
	if (!InitializeProfile(Quests, UnlockedModules, Error)) FailInitialization(Error);
}

bool UASPlayerProfileComponent::InitializeProfile(const FASQuestSaveData& Quests,
	const TArray<FName>& UnlockedModules, FString& OutError)
{
	OutError.Reset();
	if (!GetOwner() || !GetOwner()->HasAuthority() || bProfileInitialized || bInitializationFailed)
	{
		OutError = TEXT("Only an uninitialized server PlayerState can import a personal profile.");
		return false;
	}
	TArray<FName> ResolvedUnlocked;
	UASQuestComponent* QuestComponent = GetQuests();
	if (!QuestComponent || !ResolveUnlockedModules(UnlockedModules, ResolvedUnlocked, OutError)
		|| !QuestComponent->RestoreSaveData(Quests, OutError))
	{
		if (OutError.IsEmpty()) OutError = TEXT("PlayerState has no quest component.");
		return false;
	}
	PersonalUnlockedModuleIds = MoveTemp(ResolvedUnlocked);
	bProfileInitialized = true;
	GetWorld()->GetTimerManager().ClearTimer(LoadRetryTimer);
	GetOwner()->ForceNetUpdate();
	OnProfileChanged.Broadcast();
	// Persist newly introduced starting quests and any objective-definition migrations as well.
	HandleQuestsChanged();
	return true;
}

void UASPlayerProfileComponent::ServerUpdateUnlockedModules_Implementation(const TArray<FName>& UnlockedModules)
{
	if (!bProfileInitialized) return;
	FString Error;
	TArray<FName> Resolved;
	if (ResolveUnlockedModules(UnlockedModules, Resolved, Error))
	{
		PersonalUnlockedModuleIds = MoveTemp(Resolved);
	}
}

TArray<FName> UASPlayerProfileComponent::GetPersonalDropModuleIds() const
{
	TArray<FName> Result;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bProfileInitialized) return Result;
	// The server validates this cache at profile import and whenever personal unlocks change.
	return PersonalUnlockedModuleIds;
}

void UASPlayerProfileComponent::HandleLocalModulesChanged()
{
	if (bLocalProfileSubmitted && LocalCollection.IsValid())
	{
		ServerUpdateUnlockedModules(LocalCollection->GetUnlockedModuleIds());
	}
}

void UASPlayerProfileComponent::HandleQuestsChanged()
{
	if (GetOwner()->HasAuthority() && bProfileInitialized)
	{
		if (const UASQuestComponent* Quests = GetQuests()) ClientSaveQuestProgress(Quests->ExportSaveData());
	}
}

void UASPlayerProfileComponent::ServerSaveProfile_Implementation()
{
	HandleQuestsChanged();
}

void UASPlayerProfileComponent::ClientSaveQuestProgress_Implementation(const FASQuestSaveData& Quests)
{
	if (!bLocalProfileSubmitted || !LocalCollection.IsValid()) return;
	FString Error;
	if (!LocalCollection->SaveQuestProgress(Quests, Error))
	{
		ReportLocalProfileError(Error);
		return;
	}
	LastProfileError.Reset();
	OnProfileChanged.Broadcast();
}

void UASPlayerProfileComponent::FailInitialization(const FString& Error)
{
	bInitializationFailed = true;
	GetWorld()->GetTimerManager().ClearTimer(LoadRetryTimer);
	ClientReportProfileError(Error);
}

void UASPlayerProfileComponent::ServerReportLoadFailure_Implementation(const FString& Error)
{
	if (!bProfileInitialized && !bInitializationFailed) FailInitialization(Error.Left(1024));
}

void UASPlayerProfileComponent::ClientReportProfileError_Implementation(const FString& Error)
{
	ReportLocalProfileError(Error);
}

void UASPlayerProfileComponent::ReportLocalProfileError(const FString& Error)
{
	LastProfileError = Error;
	UE_LOG(LogTemp, Warning, TEXT("[PlayerProfile] %s"), *Error);
	OnProfileChanged.Broadcast();
}

void UASPlayerProfileComponent::OnRep_ProfileInitialized()
{
	OnProfileChanged.Broadcast();
}
