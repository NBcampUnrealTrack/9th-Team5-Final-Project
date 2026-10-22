#include "Core/Framework/ASLobbyGameMode.h"

#include "Core/Framework/ASLobbyPlayerController.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "Core/Framework/ASPlayerState.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

AASLobbyGameMode::AASLobbyGameMode()
{
	// The lobby displays UI without spawning a playable pawn.
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AASLobbyPlayerController::StaticClass();
	GameStateClass = AASLobbyGameState::StaticClass();
	PlayerStateClass = AASPlayerState::StaticClass();
	bUseSeamlessTravel = true;
}

void AASLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	RefreshLobby();
	UE_LOG(LogTemp, Log, TEXT("[Lobby] Player joined: %s"), *GetNameSafe(NewPlayer));
}

void AASLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	RefreshLobby(Exiting);
}

void AASLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (bStartingGame) ErrorMessage = TEXT("Game is starting.");
}

void AASLobbyGameMode::RefreshLobby(AController* ExcludedPlayer)
{
	AASLobbyGameState* State = GetGameState<AASLobbyGameState>();
	if (!State) return;
	TArray<FASLobbyPlayerInfo> Players;
	bool bAllReady = true;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AASLobbyPlayerController* Controller = Cast<AASLobbyPlayerController>(It->Get());
		if (!Controller || Controller == ExcludedPlayer || !Controller->PlayerState) continue;
		FASLobbyPlayerInfo Info;
		Info.PlayerId = Controller->PlayerState->GetPlayerId();
		Info.PlayerName = Controller->PlayerState->GetPlayerName();
		Info.bIsHost = Controller->IsLocalController();
		Info.bIsReady = Controller->IsReady();
		bAllReady &= Info.bIsReady;
		Players.Add(Info);
	}
	State->UpdateLobby(Players, !bStartingGame && Players.Num() >= MinimumPlayers && bAllReady, bStartingGame);
}

void AASLobbyGameMode::TryStartGame(APlayerController* RequestingPlayer)
{
	if (!RequestingPlayer || !RequestingPlayer->IsLocalController() || bStartingGame) return;
	RefreshLobby();
	const AASLobbyGameState* State = GetGameState<AASLobbyGameState>();
	if (!State || !State->bCanStartGame || GameMapPath.IsEmpty()) return;
	bStartingGame = true;
	RefreshLobby();
	UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>();
	if (Sessions) Sessions->SetLobbyOpen(false);
	UE_LOG(LogTemp, Log, TEXT("[Lobby] Starting game: %s"), *GameMapPath);
	if (!GetWorld()->ServerTravel(GameMapPath))
	{
		bStartingGame = false;
		if (Sessions) Sessions->SetLobbyOpen(true);
		RefreshLobby();
	}
}
