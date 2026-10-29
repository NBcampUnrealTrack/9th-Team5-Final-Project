#include "Core/Framework/ASLobbyGameState.h"
#include "Net/UnrealNetwork.h"
#include "Characters/Player/Data/ASCharacterRoster.h"

void AASLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASLobbyGameState, LobbyPlayers);
	DOREPLIFETIME(AASLobbyGameState, RoomCode);
	DOREPLIFETIME(AASLobbyGameState, bCanStartGame);
	DOREPLIFETIME(AASLobbyGameState, bGameStarting);
	DOREPLIFETIME(AASLobbyGameState, LobbyPhase);
	DOREPLIFETIME(AASLobbyGameState, CharacterRoster);
	DOREPLIFETIME(AASLobbyGameState, CharacterSelectionEndTime);
}

void AASLobbyGameState::UpdateLobby(const TArray<FASLobbyPlayerInfo>& Players, const bool bCanStart, const bool bStarting)
{
	if (!HasAuthority()) return;
	LobbyPlayers = Players;
	bCanStartGame = bCanStart;
	bGameStarting = bStarting;
	ForceNetUpdate();
	OnRep_Lobby();
}

void AASLobbyGameState::SetRoomCode(const FString& Code)
{
	if (!HasAuthority() || RoomCode == Code) return;
	RoomCode = Code;
	ForceNetUpdate();
	OnRep_Lobby();
}

void AASLobbyGameState::OnRep_Lobby()
{
	OnLobbyStateChanged.Broadcast();
}

void AASLobbyGameState::BeginCharacterSelection(const UASCharacterRoster* Roster, const float Duration)
{
	if (!HasAuthority()) return;
	CharacterRoster = Roster;
	CharacterSelectionEndTime = GetServerWorldTimeSeconds() + Duration;
	LobbyPhase = EASLobbyPhase::CharacterSelection;
	bGameStarting = true;
	bCanStartGame = false;
	ForceNetUpdate();
	OnRep_Lobby();
}

void AASLobbyGameState::MarkTravelling()
{
	if (!HasAuthority()) return;
	LobbyPhase = EASLobbyPhase::Travelling;
	ForceNetUpdate();
	OnRep_Lobby();
}

float AASLobbyGameState::GetCharacterSelectionTimeRemaining() const
{
	return LobbyPhase == EASLobbyPhase::CharacterSelection
		? FMath::Max(0.0f, CharacterSelectionEndTime - GetServerWorldTimeSeconds()) : 0.0f;
}
