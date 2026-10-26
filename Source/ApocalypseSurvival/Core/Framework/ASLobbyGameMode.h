#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ASLobbyGameMode.generated.h"

class UASCharacterRoster;
class AASLobbyPlayerController;

UCLASS()
class APOCALYPSESURVIVAL_API AASLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AASLobbyGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void PostSeamlessTravel() override;
	virtual void Logout(AController* Exiting) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	void RefreshLobby(AController* ExcludedPlayer = nullptr);
	void TryStartGame(APlayerController* RequestingPlayer);
	void SelectCharacter(AASLobbyPlayerController* Player, int32 Index);
	void SetCharacterReady(AASLobbyPlayerController* Player, bool bReady);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Lobby", meta = (ClampMin = "1"))
	int32 MinimumPlayers = 2;
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FString GameMapPath = TEXT("/Game/ApocalypseSurvival/Maps/Map_Proto");
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|CharacterSelect")
	TObjectPtr<const UASCharacterRoster> CharacterRoster;
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|CharacterSelect", meta = (ClampMin = "1.0", Units = "s"))
	float CharacterSelectionDuration = 30.0f;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bStartingGame = false;
	void FinishCharacterSelection();
	void TryFinishCharacterSelection(AController* ExcludedPlayer = nullptr);
	FTimerHandle CharacterSelectionTimer;
};
