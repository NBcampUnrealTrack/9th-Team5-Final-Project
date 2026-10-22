#include "Core/Framework/ASLobbyGameState.h"
#include "Net/UnrealNetwork.h"

void AASLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASLobbyGameState, LobbyPlayers);
	DOREPLIFETIME(AASLobbyGameState, bCanStartGame);
	DOREPLIFETIME(AASLobbyGameState, bGameStarting);
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

void AASLobbyGameState::OnRep_Lobby()
{
	OnLobbyStateChanged.Broadcast();
}
