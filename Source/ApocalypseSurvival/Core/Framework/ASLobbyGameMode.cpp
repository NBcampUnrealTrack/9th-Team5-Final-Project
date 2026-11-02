#include "Core/Framework/ASLobbyGameMode.h"

#include "Core/Framework/ASLobbyPlayerController.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "Core/Framework/ASPlayerState.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Core/Loading/ASLoadingFlowSubsystem.h"
#include "Core/Loading/ASLoadingDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Characters/Player/Data/ASCharacterRoster.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameSession.h"

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
	const AASLobbyGameState* State = GetGameState<AASLobbyGameState>();
	if (State && State->LobbyPhase == EASLobbyPhase::Travelling)
	{
		if (GameSession) GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("게임이 이미 시작되었습니다.")));
		return;
	}
	if (State && State->LobbyPhase == EASLobbyPhase::CharacterSelection)
	{
		// PreLogin may have succeeded just before selection started.
		const int32 DefaultIndex = ResolveCharacterIndex(CharacterRoster, INDEX_NONE);
		if (DefaultIndex == INDEX_NONE)
		{
			AbortCharacterSelection(FText::FromString(TEXT("사용 가능한 캐릭터가 없습니다. 캐릭터 설정을 확인해 주세요.")));
			return;
		}
		if (AASLobbyPlayerController* Player = Cast<AASLobbyPlayerController>(NewPlayer)) Player->InitializeCharacterSelection(DefaultIndex);
	}
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
	if (!State || !State->bCanStartGame) return;
	const auto* Definition = GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->GetLoadingDefinition();
	if (!Definition || Definition->LoadingMap.IsNull() || Definition->DefaultGameMap.IsNull())
	{
		AbortCharacterSelection(FText::FromString(TEXT("GameInstance에 연결된 로딩 DA에서 로딩 맵과 게임 맵을 지정해 주세요.")));
		return;
	}
	const int32 DefaultIndex = ResolveCharacterIndex(CharacterRoster, INDEX_NONE);
	if (DefaultIndex == INDEX_NONE)
	{
		AbortCharacterSelection(FText::FromString(TEXT("사용 가능한 캐릭터가 없습니다. 캐릭터 설정을 확인해 주세요.")));
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
	GetWorldTimerManager().SetTimer(CharacterSelectionTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]() { FinishCharacterSelection(); }), CharacterSelectionDuration, false);
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
	if (PlayerCount > 0) FinishCharacterSelection(ExcludedPlayer);
}

void AASLobbyGameMode::FinishCharacterSelection(AController* ExcludedPlayer)
{
	AASLobbyGameState* State = GetGameState<AASLobbyGameState>();
	if (!State || State->LobbyPhase != EASLobbyPhase::CharacterSelection) return;
	UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>();
	if (!Sessions) return;
	if (Sessions->IsLeavingRoom())
	{
		GetWorldTimerManager().ClearTimer(CharacterSelectionTimer);
		return;
	}
	TArray<TPair<AASPlayerState*, const UASCharacterDefinition*>> Selections;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AASLobbyPlayerController* Player = Cast<AASLobbyPlayerController>(It->Get());
		AASPlayerState* PlayerState = Player ? Player->GetPlayerState<AASPlayerState>() : nullptr;
		if (!PlayerState || Player == ExcludedPlayer) continue;
		const int32 FinalIndex = ResolveCharacterIndex(CharacterRoster, Player->GetCharacterIndex());
		if (FinalIndex == INDEX_NONE)
		{
			AbortCharacterSelection(FText::FromString(TEXT("사용 가능한 캐릭터가 없어 게임을 시작하지 못했습니다.")));
			return;
		}
		Player->SetCharacterSelectionOnServer(FinalIndex, Player->IsCharacterReady());
		Selections.Emplace(PlayerState, CharacterRoster->GetCharacterAt(FinalIndex));
	}
	if (Selections.IsEmpty())
	{
		AbortCharacterSelection(FText::FromString(TEXT("참가자가 없어 게임 시작을 취소했습니다.")));
		return;
	}
	Sessions->ClearMatchCharacters();
	for (const auto& Selection : Selections)
	{
		Selection.Key->SetCharacterDefinition(Selection.Value);
		Selection.Key->MarkCharacterSelectionReceived();
		Sessions->SaveMatchCharacter(Selection.Key->GetUniqueId(), Selection.Value);
		UE_LOG(LogTemp, Log, TEXT("[CharacterSelect] Final selection: %s -> %s"), *Selection.Key->GetPlayerName(), *GetNameSafe(Selection.Value));
	}
	GetWorldTimerManager().ClearTimer(CharacterSelectionTimer);
	State->MarkTravelling();
	const auto* Definition = GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->GetLoadingDefinition();
	const FString DestinationMap = Definition ? Definition->DefaultGameMap.ToSoftObjectPath().GetLongPackageName() : FString();
	UE_LOG(LogTemp, Log, TEXT("[Lobby] Starting game: %s"), *DestinationMap);
	if (!GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->TravelThroughLoadingLevel(DestinationMap))
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterSelect] Game map travel request failed."));
		AbortCharacterSelection(FText::FromString(TEXT("게임 맵으로 이동하지 못했습니다. 맵 설정을 확인한 후 다시 방을 생성해 주세요.")));
	}
}

void AASLobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CharacterSelectionTimer);
	Super::EndPlay(EndPlayReason);
}

int32 AASLobbyGameMode::ResolveCharacterIndex(const UASCharacterRoster* Roster, const int32 RequestedIndex)
{
	if (!Roster) return INDEX_NONE;
	const UASCharacterDefinition* Requested = Roster->GetCharacterAt(RequestedIndex);
	if (Requested && Requested->CharacterClass) return RequestedIndex;
	for (int32 Index = 0; Index < Roster->Characters.Num(); ++Index)
	{
		const UASCharacterDefinition* Definition = Roster->GetCharacterAt(Index);
		if (Definition && Definition->CharacterClass) return Index;
	}
	return INDEX_NONE;
}

void AASLobbyGameMode::AbortCharacterSelection(const FText& Message)
{
	GetWorldTimerManager().ClearTimer(CharacterSelectionTimer);
	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] %s"), *Message.ToString());
	if (UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>())
	{
		Sessions->ReturnToMenuWithError(Message);
	}
}
