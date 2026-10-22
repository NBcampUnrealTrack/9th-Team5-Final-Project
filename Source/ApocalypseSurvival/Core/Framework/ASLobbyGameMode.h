#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ASLobbyGameMode.generated.h"

UCLASS()
class APOCALYPSESURVIVAL_API AASLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AASLobbyGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	void RefreshLobby(AController* ExcludedPlayer = nullptr);
	void TryStartGame(APlayerController* RequestingPlayer);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Lobby", meta = (ClampMin = "1"))
	int32 MinimumPlayers = 2;
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FString GameMapPath = TEXT("/Game/ApocalypseSurvival/Maps/Map_Proto");

private:
	bool bStartingGame = false;
};
