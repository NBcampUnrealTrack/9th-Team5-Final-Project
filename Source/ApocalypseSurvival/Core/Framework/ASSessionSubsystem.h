#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ASSessionSubsystem.generated.h"

class UASCharacterDefinition;
class UASSessionInviteWidget;

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

	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinSessionByIndex(int32 ResultIndex, const FString& Password = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "Session|Steam")
	bool IsSteamSession() const;
	UFUNCTION(BlueprintPure, Category = "Session|Steam")
	bool IsSessionServiceReady() const;
	UFUNCTION(BlueprintPure, Category = "Session|Steam")
	FText GetConnectionStatusText() const;
	UFUNCTION(BlueprintPure, Category = "Session|Steam")
	bool CanInviteFriends() const;
	UFUNCTION(BlueprintCallable, Category = "Session|Steam")
	bool ShowInviteFriendsUI();
	// The invite confirmation supplies the room password without advertising it on Steam.
	bool AcceptSessionInvite(const FString& Password);
	void DismissSessionInvite();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void DestroySession();
	UFUNCTION(BlueprintCallable, Category = "Session")
	void LeaveRoom();
	UFUNCTION(BlueprintCallable, Category = "Session")
	void CancelJoin();
	void ReturnToMenuWithError(const FText& Message);
	bool IsLeavingRoom() const { return bLeavingRoom; }
	static bool ShouldEndHostSessionForNetworkFailure(ENetworkFailure::Type FailureType);
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
	friend class FASSessionCleanupTest;
	friend class FASJoinCancellationTest;
	friend class FASSteamSessionSettingsTest;
	friend class FASSteamInviteValidationTest;
	static FOnlineSessionSettings MakeSessionSettings(bool bSteam, int32 PublicConnections, const FString& RoomName, bool bRequiresPassword);
	static void ConfigureSearch(FOnlineSessionSearch& Search, bool bSteam, int32 MaxResults);
	static bool IsCompatibleSession(const FOnlineSessionSearchResult& Result);
	void JoinSessionResult(const FOnlineSessionSearchResult& SearchResult, const FString& Password);
	void HandleSessionInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void ProcessPendingInvite(UWorld* World);
	FUniqueNetIdRepl GetLocalUserId() const;

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	void HandleMapLoaded(UWorld* World);
	void HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	void HandleJoinTimeout();
	void HandleDestroyTimeout();
	void StopJoinAttempt();
	void HandleConnectionFailure(UWorld* World, const FString& Error);
	bool bJoinTravelPending = false;
	FString HostPassword;
	FString PendingJoinPassword;
	bool bLeavingRoom = false;
	bool bJoiningSession = false;
	bool bCreatingSession = false;
	bool bDestroyingSession = false;
	bool bFindingSessions = false;
	FTimerHandle JoinTimeoutTimer;
	FTimerHandle DestroyTimeoutTimer;
	FTimerHandle ReturnToMenuTimer;
	void FinishLeavingRoom();
	// Server-owned choices survive non-seamless reconnects as well as normal map travel.
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<const UASCharacterDefinition>> MatchCharacters;

	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	TOptional<FOnlineSessionSearchResult> PendingInvite;
	FString AcceptedInvitePassword;
	bool bInviteAccepted = false;
	UPROPERTY(Transient)
	TObjectPtr<UASSessionInviteWidget> InviteWidget;
};
