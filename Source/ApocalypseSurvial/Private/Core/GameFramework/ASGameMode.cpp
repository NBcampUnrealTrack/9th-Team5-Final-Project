#include "Core/GameFramework/ASGameMode.h"
#include "Core/GameFramework/ASGameState.h"

AASGameMode::AASGameMode()
{
	GameStateClass = AASGameState::StaticClass();
}
