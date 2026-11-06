#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "ASLoadingLevel.generated.h"

UCLASS()
class APOCALYPSESURVIVAL_API AASLoadingLevelGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Loading") FString DestinationMap;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Loading") FGuid TravelId;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Loading") float PreparationTimeout = 120.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Loading") bool bPreparationFailed = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Loading") FText FailureMessage;
};

/** Replaces the lobby controller during the real loading world; no playable pawn is spawned here. */
UCLASS()
class APOCALYPSESURVIVAL_API AASLoadingLevelPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void PlayerTick(float DeltaTime) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	FGuid LocalTravelId;
	bool bReportedReady = false;
	bool bReportedFailure = false;
	UFUNCTION(Server, Reliable) void ServerReportPreparation(FGuid TravelId, bool bReady, bool bFailed);
	UFUNCTION() void HandlePreparationFailed(FText Error);
};

UCLASS()
class APOCALYPSESURVIVAL_API AASLoadingLevelGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AASLoadingLevelGameMode();
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void Logout(AController* Exiting) override;
	void RecordPreparation(AASLoadingLevelPlayerController* Player, FGuid TravelId, bool bReady, bool bFailed);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	FTimerHandle PreparationTimer;
	TSet<TWeakObjectPtr<AASLoadingLevelPlayerController>> PreparedPlayers;
	int32 ExpectedPlayerCount = 1;
	double PreparationStartedAt = 0;
	bool bFinished = false;
	void PollPreparation();
	void AbortPreparation(const FText& Message);
};
