#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ASSessionSubsystem.generated.h"

class UASCharacterDefinition;

USTRUCT(BlueprintType)
struct FASSessionSearchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bRequiresPassword = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASSessionOperationCompleted, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FASSessionSearchCompleted,
	bool,
	bSuccess,
	const TArray<FASSessionSearchResult>&,
	Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FASSessionJoinCompleted,
	bool,
	bSuccess,
	const FString&,
	ErrorMessage);

UCLASS()
class APOCALYPSESURVIVAL_API UASSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Session")
	void CreateSession(int32 PublicConnections, const FString& RoomName, bool bUsePassword = false, const FString& Password = TEXT(""));
	static bool IsValidRoomPassword(const FString& Password);
	bool ValidateRoomPassword(const FString& Password) const;

	UFUNCTION(BlueprintCallable, Category = "Session")
	void FindSessions(int32 MaxResults = 100);

	UFUNCTION(BlueprintCallable, Category = "Session|LAN")
	void JoinSessionByIndex(int32 ResultIndex, const FString& Password = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Session")
	void DestroySession();
	UFUNCTION(BlueprintCallable, Category = "Session")
	void LeaveRoom();
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FText ReturnToMenuMessage;

	UFUNCTION(BlueprintCallable, Category = "Session|Travel")
	bool HostTravelToMap(const FString& MapPath);
	void SetLobbyOpen(bool bOpen);
	void SaveMatchCharacter(const FUniqueNetIdRepl& PlayerId, const UASCharacterDefinition* Definition);
	const UASCharacterDefinition* FindMatchCharacter(const FUniqueNetIdRepl& PlayerId) const;
	void ClearMatchCharacters();

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FASSessionOperationCompleted OnSessionCreated;

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FASSessionSearchCompleted OnSessionsFound;

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FASSessionJoinCompleted OnSessionJoined;

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FASSessionOperationCompleted OnSessionDestroyed;

private:
	FUniqueNetIdRepl GetLocalUserId() const;

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	void HandleMapLoaded(UWorld* World);
	void HandleConnectionFailure(UWorld* World, const FString& Error);
	bool bJoinTravelPending = false;
	FString HostPassword;
	FString PendingJoinPassword;
	bool bLeavingRoom = false;
	void FinishLeavingRoom(bool bSessionDestroyed);
	// Server-owned choices survive non-seamless reconnects as well as normal map travel.
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<const UASCharacterDefinition>> MatchCharacters;

	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
};
