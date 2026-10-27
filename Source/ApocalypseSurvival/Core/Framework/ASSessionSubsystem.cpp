#include "Core/Framework/ASSessionSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "GameMapsSettings.h"
#include "TimerManager.h"
#include "Core/UI/ASLoadingScreenSubsystem.h"

namespace
{
	const FName RoomNameKey(TEXT("RoomName"));
	const FName RequiresPasswordKey(TEXT("RequiresPassword"));
}

void UASSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandleMapLoaded);

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] Online session interface를 찾지 못했습니다."));
	}
}

void UASSessionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
		SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
	}

	SessionSearch.Reset();
	SessionInterface.Reset();

	Super::Deinitialize();
}

FUniqueNetIdRepl UASSessionSubsystem::GetLocalUserId() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();
}

bool UASSessionSubsystem::IsValidRoomPassword(const FString& Password)
{
	if (Password.Len() != 4) return false;
	for (const TCHAR Character : Password)
	{
		if (Character < TEXT('0') || Character > TEXT('9')) return false;
	}
	return true;
}

bool UASSessionSubsystem::ValidateRoomPassword(const FString& Password) const
{
	return HostPassword.IsEmpty() || (IsValidRoomPassword(Password) && Password == HostPassword);
}

void UASSessionSubsystem::CreateSession(const int32 PublicConnections, const FString& RoomName, const bool bUsePassword, const FString& Password)
{
	if (bUsePassword && !IsValidRoomPassword(Password))
	{
		OnSessionCreated.Broadcast(false);
		return;
	}
	if (!SessionInterface.IsValid())
	{
		OnSessionCreated.Broadcast(false);
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] 이미 GameSession이 존재합니다."));
		OnSessionCreated.Broadcast(false);
		return;
	}

	const FUniqueNetIdRepl UserId = GetLocalUserId();
	if (!UserId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] 로컬 사용자 ID를 찾지 못했습니다."));
		OnSessionCreated.Broadcast(false);
		return;
	}

	FOnlineSessionSettings SessionSettings;
	HostPassword = bUsePassword ? Password : FString();
	ReturnToMenuMessage = FText::GetEmpty();
	SessionSettings.Set(RequiresPasswordKey, bUsePassword, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	ClearMatchCharacters();
	SessionSettings.NumPublicConnections = FMath::Max(2, PublicConnections);
	SessionSettings.bIsLANMatch = true;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bAllowJoinViaPresence = false;
	SessionSettings.bUsesPresence = false;
	SessionSettings.bUseLobbiesIfAvailable = false;
	SessionSettings.Set(
		RoomNameKey,
		RoomName,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(
		this,
		&ThisClass::HandleCreateSessionComplete);

	if (!SessionInterface->CreateSession(*UserId, NAME_GameSession, SessionSettings))
	{
		SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
		OnSessionCreated.Broadcast(false);
	}
}

void UASSessionSubsystem::FindSessions(const int32 MaxResults)
{
	if (!SessionInterface.IsValid())
	{
		OnSessionsFound.Broadcast(false, TArray<FASSessionSearchResult>());
		return;
	}

	const FUniqueNetIdRepl UserId = GetLocalUserId();
	if (!UserId.IsValid())
	{
		OnSessionsFound.Broadcast(false, TArray<FASSessionSearchResult>());
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = true;
	SessionSearch->MaxSearchResults = FMath::Max(1, MaxResults);

	SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
	SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(
		this,
		&ThisClass::HandleFindSessionsComplete);

	if (!SessionInterface->FindSessions(*UserId, SessionSearch.ToSharedRef()))
	{
		SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
		SessionSearch.Reset();
		OnSessionsFound.Broadcast(false, TArray<FASSessionSearchResult>());
	}
}

void UASSessionSubsystem::JoinSessionByIndex(const int32 ResultIndex, const FString& Password)
{
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
	{
		OnSessionJoined.Broadcast(false, TEXT("이미 세션에 참가해 있습니다."));
		return;
	}
	if (!SessionInterface.IsValid() || !SessionSearch.IsValid()
		|| !SessionSearch->SearchResults.IsValidIndex(ResultIndex))
	{
		OnSessionJoined.Broadcast(false, TEXT("참가할 세션을 찾지 못했습니다."));
		return;
	}

	const FUniqueNetIdRepl UserId = GetLocalUserId();
	if (!UserId.IsValid())
	{
		OnSessionJoined.Broadcast(false, TEXT("로컬 사용자 ID를 찾지 못했습니다."));
		return;
	}

	bool bRequiresPassword = false;
	SessionSearch->SearchResults[ResultIndex].Session.SessionSettings.Get(RequiresPasswordKey, bRequiresPassword);
	if (bRequiresPassword && !IsValidRoomPassword(Password))
	{
		OnSessionJoined.Broadcast(false, TEXT("비밀번호는 숫자 4자리로 입력해 주세요."));
		return;
	}
	PendingJoinPassword = bRequiresPassword ? Password : FString();
	SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);
	SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(
		this,
		&ThisClass::HandleJoinSessionComplete);

	if (!SessionInterface->JoinSession(
		*UserId,
		NAME_GameSession,
		SessionSearch->SearchResults[ResultIndex]))
	{
		SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);
		OnSessionJoined.Broadcast(false, TEXT("세션 참가 요청을 시작하지 못했습니다."));
	}
}

void UASSessionSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		OnSessionDestroyed.Broadcast(false);
		FinishLeavingRoom(false);
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		OnSessionDestroyed.Broadcast(true);
		FinishLeavingRoom(true);
		return;
	}

	SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
	SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(
		this,
		&ThisClass::HandleDestroySessionComplete);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
		OnSessionDestroyed.Broadcast(false);
		FinishLeavingRoom(false);
	}
}

void UASSessionSubsystem::LeaveRoom()
{
	if (bLeavingRoom || !GetWorld()) return;
	bLeavingRoom = true;
	bJoinTravelPending = false;
	ReturnToMenuMessage = FText::GetEmpty();
	if (GetWorld()->GetNetMode() == NM_ListenServer) SetLobbyOpen(false);
	DestroySession();
}

void UASSessionSubsystem::FinishLeavingRoom(const bool bSessionDestroyed)
{
	if (!bLeavingRoom) return;
	if (!bSessionDestroyed && SessionInterface.IsValid())
	{
		// Do not leave a stale local session blocking the next create/join attempt.
		SessionInterface->RemoveNamedSession(NAME_GameSession);
	}
	ClearMatchCharacters();
	SessionSearch.Reset();
	HostPassword.Reset();
	PendingJoinPassword.Reset();
	// Network failure callbacks may still be processing their own travel this frame.
	GetGameInstance()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UGameplayStatics::OpenLevel(GetGameInstance(), FName(*UGameMapsSettings::GetGameDefaultMap()), true);
	}));
}

bool UASSessionSubsystem::HostTravelToMap(const FString& MapPath)
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode() || MapPath.IsEmpty())
	{
		return false;
	}

	FString TravelURL = MapPath;
	if (!TravelURL.Contains(TEXT("?")))
	{
		TravelURL += TEXT("?listen");
	}
	else if (!TravelURL.Contains(TEXT("listen")))
	{
		TravelURL += TEXT("?listen");
	}

	return World->ServerTravel(TravelURL);
}

void UASSessionSubsystem::SetLobbyOpen(const bool bOpen)
{
	if (!SessionInterface.IsValid()) return;
	const FOnlineSessionSettings* CurrentSettings = SessionInterface->GetSessionSettings(NAME_GameSession);
	if (!CurrentSettings) return;
	FOnlineSessionSettings Settings = *CurrentSettings;
	Settings.bShouldAdvertise = bOpen;
	Settings.bAllowJoinInProgress = bOpen;
	if (!SessionInterface->UpdateSession(NAME_GameSession, Settings))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] Failed to update lobby advertisement."));
	}
}

void UASSessionSubsystem::SaveMatchCharacter(const FUniqueNetIdRepl& PlayerId, const UASCharacterDefinition* Definition)
{
	if (PlayerId.IsValid() && Definition) MatchCharacters.Add(PlayerId.ToString(), Definition);
}

const UASCharacterDefinition* UASSessionSubsystem::FindMatchCharacter(const FUniqueNetIdRepl& PlayerId) const
{
	const TObjectPtr<const UASCharacterDefinition>* Definition = PlayerId.IsValid() ? MatchCharacters.Find(PlayerId.ToString()) : nullptr;
	return Definition ? Definition->Get() : nullptr;
}

void UASSessionSubsystem::ClearMatchCharacters()
{
	MatchCharacters.Reset();
}

void UASSessionSubsystem::HandleCreateSessionComplete(
	const FName SessionName,
	const bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 생성: %s"), bWasSuccessful ? TEXT("성공") : TEXT("실패"));
	OnSessionCreated.Broadcast(bWasSuccessful);
}

void UASSessionSubsystem::HandleFindSessionsComplete(const bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
	}

	TArray<FASSessionSearchResult> Results;
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& SearchResult = SessionSearch->SearchResults[Index];
			FASSessionSearchResult Result;
			Result.Index = Index;
			Result.RoomName = SearchResult.Session.OwningUserName;
			SearchResult.Session.SessionSettings.Get(RoomNameKey, Result.RoomName);
			Result.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
			SearchResult.Session.SessionSettings.Get(RequiresPasswordKey, Result.bRequiresPassword);
			Result.CurrentPlayers = Result.MaxPlayers - SearchResult.Session.NumOpenPublicConnections;
			Results.Add(Result);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 검색: %d개"), Results.Num());
	OnSessionsFound.Broadcast(bWasSuccessful, Results);
}

void UASSessionSubsystem::HandleJoinSessionComplete(
	const FName SessionName,
	const EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);
	}

	if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface.IsValid())
	{
		OnSessionJoined.Broadcast(false, TEXT("세션 참가에 실패했습니다."));
		return;
	}

	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		DestroySession();
		OnSessionJoined.Broadcast(false, TEXT("호스트 주소를 확인하지 못했습니다."));
		return;
	}

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		DestroySession();
		OnSessionJoined.Broadcast(false, TEXT("플레이어 컨트롤러를 찾지 못했습니다."));
		return;
	}

	bJoinTravelPending = true;
	if (!PendingJoinPassword.IsEmpty()) ConnectString += TEXT("?RoomPassword=") + PendingJoinPassword;
	PendingJoinPassword.Reset();
	GetGameInstance()->GetSubsystem<UASLoadingScreenSubsystem>()->ShowLoadingScreen();
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	OnSessionJoined.Broadcast(true, FString());
}

void UASSessionSubsystem::HandleDestroySessionComplete(
	const FName SessionName,
	const bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 종료: %s"), bWasSuccessful ? TEXT("성공") : TEXT("실패"));
	OnSessionDestroyed.Broadcast(bWasSuccessful);
	FinishLeavingRoom(bWasSuccessful);
}

void UASSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
	HandleConnectionFailure(World, Error.IsEmpty() ? TEXT("서버 연결에 실패했습니다.") : Error);
}

void UASSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || bLeavingRoom) return;
	bLeavingRoom = true;
	bJoinTravelPending = false;
	ReturnToMenuMessage = FText::FromString(TEXT("맵 이동에 실패하여 메인메뉴로 돌아왔습니다. 다시 방을 생성하거나 참가해 주세요."));
	UE_LOG(LogTemp, Warning, TEXT("[Session] Travel failed: %s"), *Error);
	SetLobbyOpen(false);
	DestroySession();
}

void UASSessionSubsystem::HandleMapLoaded(UWorld* World)
{
	if (World && World->GetGameInstance() == GetGameInstance() && World->GetNetMode() == NM_Standalone)
	{
		bLeavingRoom = false;
	}
	if (World && World->GetGameInstance() == GetGameInstance() && World->GetNetMode() == NM_Client)
	{
		bJoinTravelPending = false;
	}
}

void UASSessionSubsystem::HandleConnectionFailure(UWorld* World, const FString& Error)
{
	if (!World || World->GetGameInstance() != GetGameInstance()) return;
	if (bLeavingRoom) return;
	UE_LOG(LogTemp, Warning, TEXT("[Session] %s"), *Error);
	const bool bWasJoining = bJoinTravelPending;
	bJoinTravelPending = false;
	const bool bWasConnected = World->GetNetMode() == NM_Client;
	if (bWasConnected || bWasJoining)
	{
		bLeavingRoom = true;
		ReturnToMenuMessage = FText::FromString(bWasJoining && Error.Contains(TEXT("AS_WRONG_PASSWORD"))
			? TEXT("비밀번호가 일치하지 않습니다. 방을 다시 선택해 주세요.")
			: Error.Contains(TEXT("AS_GAME_IN_PROGRESS")) || Error.Contains(TEXT("Game is starting."))
				? TEXT("이미 시작한 방에는 참가할 수 없습니다. 다른 방을 선택해 주세요.")
				: TEXT("서버와 연결이 종료되었습니다. 방장이 나갔거나 연결이 끊겼습니다."));
	}
	DestroySession();
	if (bWasJoining) OnSessionJoined.Broadcast(false, Error);
}
