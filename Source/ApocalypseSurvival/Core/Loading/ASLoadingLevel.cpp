#include "Core/Loading/ASLoadingLevel.h"
#include "Core/Loading/ASLoadingFlowSubsystem.h"
#include "Core/Loading/ASLoadingDefinition.h"
#include "Core/Framework/ASPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

void AASLoadingLevelGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASLoadingLevelGameState, DestinationMap);
	DOREPLIFETIME(AASLoadingLevelGameState, TravelId);
	DOREPLIFETIME(AASLoadingLevelGameState, PreparationTimeout);
	DOREPLIFETIME(AASLoadingLevelGameState, bPreparationFailed);
	DOREPLIFETIME(AASLoadingLevelGameState, FailureMessage);
}

void AASLoadingLevelPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		bShowMouseCursor = true;
		SetInputMode(FInputModeUIOnly());
		GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->OnLoadingFailed.AddUniqueDynamic(this, &ThisClass::HandlePreparationFailed);
	}
}

void AASLoadingLevelPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController() || GetWorld()->IsInSeamlessTravel()) return;
	const auto* State = GetWorld()->GetGameState<AASLoadingLevelGameState>();
	if (!State || !State->TravelId.IsValid() || State->DestinationMap.IsEmpty()) return;
	auto* Flow = GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>();
	if (State->bPreparationFailed)
	{
		Flow->ReportLoadingLevelFailure(State->FailureMessage);
		return;
	}
	if (LocalTravelId != State->TravelId)
	{
		LocalTravelId = State->TravelId;
		bReportedReady = false;
		bReportedFailure = false;
		Flow->BeginLoadingLevelPreparation(State->DestinationMap, LocalTravelId, State->PreparationTimeout);
	}
	if (Flow->GetStatus().Phase == EASLoadingPhase::Failed)
	{
		HandlePreparationFailed(Flow->GetStatus().Message);
		return;
	}
	const bool bReady = Flow->IsLoadingLevelPrepared();
	if (bReady != bReportedReady && !bReportedFailure)
	{
		bReportedReady = bReady;
		ServerReportPreparation(LocalTravelId, bReady, false);
	}
}

void AASLoadingLevelPlayerController::HandlePreparationFailed(FText Error)
{
	if (bReportedFailure || !LocalTravelId.IsValid()) return;
	bReportedFailure = true;
	ServerReportPreparation(LocalTravelId, false, true);
}

void AASLoadingLevelPlayerController::ServerReportPreparation_Implementation(FGuid TravelId, bool bReady, bool bFailed)
{
	if (auto* Mode = GetWorld()->GetAuthGameMode<AASLoadingLevelGameMode>()) Mode->RecordPreparation(this, TravelId, bReady, bFailed);
}

void AASLoadingLevelPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController()) GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->OnLoadingFailed.RemoveDynamic(this, &ThisClass::HandlePreparationFailed);
	Super::EndPlay(EndPlayReason);
}

AASLoadingLevelGameMode::AASLoadingLevelGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AASLoadingLevelPlayerController::StaticClass();
	PlayerStateClass = AASPlayerState::StaticClass();
	GameStateClass = AASLoadingLevelGameState::StaticClass();
	bUseSeamlessTravel = true;
}

void AASLoadingLevelGameMode::BeginPlay()
{
	Super::BeginPlay();
	auto* Flow = GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>();
	auto* State = GetGameState<AASLoadingLevelGameState>();
	const auto* Definition = Flow->GetLoadingDefinition();
	if (!Definition)
	{
		AbortPreparation(NSLOCTEXT("ASLoadingLevel", "MissingDefinition", "Assign a Loading Definition data asset in the GameInstance Blueprint defaults."));
		return;
	}
	State->DestinationMap = Flow->GetPendingDestination().IsEmpty()
		? Definition->DefaultGameMap.ToSoftObjectPath().GetLongPackageName() : Flow->GetPendingDestination();
	State->TravelId = Flow->GetPendingTravelId().IsValid() ? Flow->GetPendingTravelId() : FGuid::NewGuid();
	State->PreparationTimeout = FMath::Max(1.f, Definition->TimeoutSeconds);
	ExpectedPlayerCount = FMath::Max(1, Flow->GetExpectedPlayers());
	PreparationStartedAt = FPlatformTime::Seconds();
	State->ForceNetUpdate();
	if (!FPackageName::IsValidLongPackageName(State->DestinationMap)
		|| !FPackageName::DoesPackageExist(State->DestinationMap)
		|| Flow->IsLoadingLevelMap(State->DestinationMap))
	{
		AbortPreparation(NSLOCTEXT("ASLoadingLevel", "UnconfiguredDestination", "Select a valid Default Game Map in the Loading Definition data asset, or supply a destination when travelling."));
		return;
	}
	// Start after all loading-map actors have had their BeginPlay opportunity to bind feature events.
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		const auto* LoadingState = GetGameState<AASLoadingLevelGameState>();
		GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->BeginLoadingLevelPreparation(
			LoadingState->DestinationMap, LoadingState->TravelId, LoadingState->PreparationTimeout);
	}));
	GetWorldTimerManager().SetTimer(PreparationTimer, this, &ThisClass::PollPreparation, 0.1f, true);
	UE_LOG(LogTemp, Log, TEXT("[LoadingLevel] Entered actual loading world; waiting for %d players"), ExpectedPlayerCount);
}

void AASLoadingLevelGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty()) ErrorMessage = TEXT("Game preparation is already in progress.");
}

void AASLoadingLevelGameMode::RecordPreparation(AASLoadingLevelPlayerController* Player, FGuid TravelId, bool bReady, bool bFailed)
{
	const auto* State = GetGameState<AASLoadingLevelGameState>();
	if (bFinished || !Player || Player->GetWorld() != GetWorld() || !State || State->TravelId != TravelId) return;
	if (bFailed)
	{
		AbortPreparation(NSLOCTEXT("ASLoadingLevel", "ClientFailed", "A player could not prepare the game. Return to the menu and try again."));
		return;
	}
	if (bReady) PreparedPlayers.Add(Player);
	else PreparedPlayers.Remove(Player);
	UE_LOG(LogTemp, Log, TEXT("[LoadingLevel] Prepared players: %d / %d"), PreparedPlayers.Num(), ExpectedPlayerCount);
}

void AASLoadingLevelGameMode::PollPreparation()
{
	if (bFinished) return;
	auto* State = GetGameState<AASLoadingLevelGameState>();
	auto* Flow = GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>();
	if (Flow->GetStatus().Phase == EASLoadingPhase::Failed)
	{
		AbortPreparation(Flow->GetStatus().Message);
		return;
	}
	if (FPlatformTime::Seconds() - PreparationStartedAt > State->PreparationTimeout)
	{
		AbortPreparation(NSLOCTEXT("ASLoadingLevel", "Timeout", "Timed out waiting for all players. Return to the menu and try again."));
		return;
	}
	if (!Flow->IsLoadingLevelPrepared() || GetWorld()->IsInSeamlessTravel()) return;
	int32 PresentPlayers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* Player = Cast<AASLoadingLevelPlayerController>(It->Get());
		if (!Player || !Player->PlayerState || !Player->HasClientLoadedCurrentWorld() || !PreparedPlayers.Contains(Player)) return;
		++PresentPlayers;
	}
	if (PresentPlayers != ExpectedPlayerCount) return;
	if (!FPackageName::DoesPackageExist(State->DestinationMap) || Flow->IsLoadingLevelMap(State->DestinationMap))
	{
		AbortPreparation(NSLOCTEXT("ASLoadingLevel", "MissingMap", "The destination game map is invalid."));
		return;
	}
	bFinished = true;
	GetWorldTimerManager().ClearTimer(PreparationTimer);
	UE_LOG(LogTemp, Log, TEXT("[LoadingLevel] ALL READY; Loading level -> %s"), *State->DestinationMap);
	const FString URL = State->DestinationMap + (GetNetMode() == NM_ListenServer ? TEXT("?listen") : TEXT(""));
	if (!GetWorld()->ServerTravel(URL, true))
	{
		bFinished = false;
		AbortPreparation(NSLOCTEXT("ASLoadingLevel", "TravelFailed", "Could not travel to the game map."));
	}
}

void AASLoadingLevelGameMode::AbortPreparation(const FText& Message)
{
	if (bFinished) return;
	bFinished = true;
	GetWorldTimerManager().ClearTimer(PreparationTimer);
	auto* State = GetGameState<AASLoadingLevelGameState>();
	State->FailureMessage = Message;
	State->bPreparationFailed = true;
	State->ForceNetUpdate();
	GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->ReportLoadingLevelFailure(Message);
}

void AASLoadingLevelGameMode::Logout(AController* Exiting)
{
	// Swapping a lobby controller for a loading controller is not a disconnect.
	if (!bFinished && Cast<AASLoadingLevelPlayerController>(Exiting))
		AbortPreparation(NSLOCTEXT("ASLoadingLevel", "Disconnected", "A player disconnected during game preparation."));
	Super::Logout(Exiting);
}

void AASLoadingLevelGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PreparationTimer);
	Super::EndPlay(EndPlayReason);
}
