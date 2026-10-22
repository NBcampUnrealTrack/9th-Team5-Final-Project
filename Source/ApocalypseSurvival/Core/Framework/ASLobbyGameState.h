#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ASLobbyGameState.generated.h"

USTRUCT(BlueprintType)
struct FASLobbyPlayerInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	int32 PlayerId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly)
	FString PlayerName;
	UPROPERTY(BlueprintReadOnly)
	bool bIsHost = false;
	UPROPERTY(BlueprintReadOnly)
	bool bIsReady = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASLobbyStateChanged);

UCLASS()
class APOCALYPSESURVIVAL_API AASLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void UpdateLobby(const TArray<FASLobbyPlayerInfo>& Players, bool bCanStart, bool bStarting);
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Lobby, Category = "Lobby")
	TArray<FASLobbyPlayerInfo> LobbyPlayers;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Lobby, Category = "Lobby")
	bool bCanStartGame = false;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Lobby, Category = "Lobby")
	bool bGameStarting = false;
	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FASLobbyStateChanged OnLobbyStateChanged;
private:
	UFUNCTION()
	void OnRep_Lobby();
};
