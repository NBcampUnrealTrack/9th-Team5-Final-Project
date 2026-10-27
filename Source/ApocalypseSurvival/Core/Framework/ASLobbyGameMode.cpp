#include "Core/Framework/ASLobbyGameMode.h"

#include "Core/Framework/ASLobbyPlayerController.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "Core/Framework/ASPlayerState.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Characters/Player/Data/ASCharacterRoster.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

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

void AASLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);
	if (AASPlayerState* State = C ? C->GetPlayerState<AASPlayerState>() : nullptr) State->ResetForNewMatch();
	RefreshLobby();
}

void AASLobbyGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	if (UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>())
	{
		Sessions->ClearMatchCharacters();
		Sessions->SetLobbyOpen(true);
	}
	RefreshLobby();
}

void AASLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	RefreshLobby(Exiting);
	TryFinishCharacterSelection(Exiting);
}

void AASLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty()) return;
	if (const UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>())
	{
		if (!Sessions->ValidateRoomPassword(UGameplayStatics::ParseOption(Options, TEXT("RoomPassword"))))
		{
			ErrorMessage = TEXT("AS_WRONG_PASSWORD");
			return;
		}
	}
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
		Info.CharacterIndex = Controller->GetCharacterIndex();
		Info.bCharacterReady = Controller->IsCharacterReady();
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
	int32 DefaultIndex = INDEX_NONE;
	if (CharacterRoster)
	{
		for (int32 Index = 0; Index < CharacterRoster->Characters.Num(); ++Index)
		{
			const UASCharacterDefinition* Definition = CharacterRoster->GetCharacterAt(Index);
			if (Definition && Definition->CharacterClass)
			{
				DefaultIndex = Index;
				break;
			}
		}
	}
	if (DefaultIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterSelect] Set a CharacterRoster with a valid character on the lobby GameMode."));
		return;
	}
	bStartingGame = true;
	UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>();
	if (Sessions) Sessions->SetLobbyOpen(false);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AASLobbyPlayerController* Player = Cast<AASLobbyPlayerController>(It->Get()))
		{
			Player->InitializeCharacterSelection(DefaultIndex);
		}
	}
	RefreshLobby();
	GetGameState<AASLobbyGameState>()->BeginCharacterSelection(CharacterRoster, CharacterSelectionDuration);
	GetWorldTimerManager().SetTimer(CharacterSelectionTimer, this, &ThisClass::FinishCharacterSelection, CharacterSelectionDuration, false);
	UE_LOG(LogTemp, Log, TEXT("[CharacterSelect] Selection started: %.1f seconds"), CharacterSelectionDuration);
}

void AASLobbyGameMode::SelectCharacter(AASLobbyPlayerController* Player, const int32 Index)
{
	const AASLobbyGameState* State = GetGameState<AASLobbyGameState>();
	if (!Player || !Player->PlayerState || Player->IsCharacterReady() || !State
		|| State->LobbyPhase != EASLobbyPhase::CharacterSelection) return;
	if (State->GetCharacterSelectionTimeRemaining() <= 0.0f)
	{
		FinishCharacterSelection();
		return;
	}
	const UASCharacterDefinition* Definition = CharacterRoster ? CharacterRoster->GetCharacterAt(Index) : nullptr;
	if (!Definition || !Definition->CharacterClass) return;
	Player->SetCharacterSelectionOnServer(Index, false);
	RefreshLobby();
}

void AASLobbyGameMode::SetCharacterReady(AASLobbyPlayerController* Player, const bool bReady)
{
	const AASLobbyGameState* State = GetGameState<AASLobbyGameState>();
	if (!Player || !Player->PlayerState || !State || State->LobbyPhase != EASLobbyPhase::CharacterSelection) return;
	if (State->GetCharacterSelectionTimeRemaining() <= 0.0f)
	{
		FinishCharacterSelection();
		return;
	}
	const UASCharacterDefinition* Definition = CharacterRoster ? CharacterRoster->GetCharacterAt(Player->GetCharacterIndex()) : nullptr;
	if (!Definition || !Definition->CharacterClass) return;
	Player->SetCharacterSelectionOnServer(Player->GetCharacterIndex(), bReady);
	RefreshLobby();
	TryFinishCharacterSelection();
}

void AASLobbyGameMode::TryFinishCharacterSelection(AController* ExcludedPlayer)
{
	const AASLobbyGameState* State = GetGameState<AASLobbyGameState>();
	if (!State || State->LobbyPhase != EASLobbyPhase::CharacterSelection) return;
	int32 PlayerCount = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AASLobbyPlayerController* Player = Cast<AASLobbyPlayerController>(It->Get());
		if (!Player || Player == ExcludedPlayer || !Player->PlayerState) continue;
		++PlayerCount;
		if (!Player->IsCharacterReady()) return;
	}
	if (PlayerCount > 0) FinishCharacterSelection();
}

void AASLobbyGameMode::FinishCharacterSelection()
{
	AASLobbyGameState* State = GetGameState<AASLobbyGameState>();
	if (!State || State->LobbyPhase != EASLobbyPhase::CharacterSelection) return;
	UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>();
	if (!Sessions) return;
	Sessions->ClearMatchCharacters();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AASLobbyPlayerController* Player = Cast<AASLobbyPlayerController>(It->Get());
		AASPlayerState* PlayerState = Player ? Player->GetPlayerState<AASPlayerState>() : nullptr;
		if (!PlayerState) continue;
		const UASCharacterDefinition* Definition = CharacterRoster ? CharacterRoster->GetCharacterAt(Player->GetCharacterIndex()) : nullptr;
		if (!Definition || !Definition->CharacterClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[CharacterSelect] Invalid server selection for %s"), *GetNameSafe(Player));
			return;
		}
		PlayerState->SetCharacterDefinition(Definition);
		PlayerState->MarkCharacterSelectionReceived();
		Sessions->SaveMatchCharacter(PlayerState->GetUniqueId(), Definition);
		UE_LOG(LogTemp, Log, TEXT("[CharacterSelect] Final selection: %s -> %s"), *PlayerState->GetPlayerName(), *GetNameSafe(Definition));
	}
	GetWorldTimerManager().ClearTimer(CharacterSelectionTimer);
	State->MarkTravelling();
	UE_LOG(LogTemp, Log, TEXT("[Lobby] Starting game: %s"), *GameMapPath);
	// Absolute travel clears old options, so listen must be supplied again for non-seamless travel.
	const FString TravelURL = GameMapPath + TEXT("?listen");
	if (!GetWorld()->ServerTravel(TravelURL, true))
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterSelect] Game map travel request failed."));
		Sessions->LeaveRoom();
		Sessions->ReturnToMenuMessage = FText::FromString(TEXT("게임 맵으로 이동하지 못했습니다. 맵 설정을 확인한 후 다시 방을 생성해 주세요."));
	}
}

void AASLobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CharacterSelectionTimer);
	Super::EndPlay(EndPlayReason);
}
