#include "Core/Framework/ASSessionSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Core/UI/ASSessionInviteWidget.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "GameMapsSettings.h"
#include "TimerManager.h"
#include "Core/UI/ASLoadingScreenSubsystem.h"

namespace
{
	const FName RoomNameKey(TEXT("RoomName"));
	const FName RequiresPasswordKey(TEXT("RequiresPassword"));
	const FName GameIdKey(TEXT("ASGame"));
	const FName LobbyOpenKey(TEXT("LobbyOpen"));
	// Also isolates our lobbies from other projects using the shared test App ID 480.
	constexpr int32 GameProtocolId = 0x41535001;
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
	FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &ThisClass::HandleWorldPostActorTick);

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnSessionUserInviteAcceptedDelegates.AddUObject(this, &ThisClass::HandleSessionInviteAccepted);
	}
	UE_LOG(LogTemp, Log, TEXT("[Session] Backend=%s: %s"),
		OnlineSubsystem ? *OnlineSubsystem->GetSubsystemName().ToString() : TEXT("None"), *GetConnectionStatusText().ToString());

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] Online session interface를 찾지 못했습니다."));
	}
}

void UASSessionSubsystem::Deinitialize()
{
	GetGameInstance()->GetTimerManager().ClearTimer(JoinTimeoutTimer);
	GetGameInstance()->GetTimerManager().ClearTimer(DestroyTimeoutTimer);
	GetGameInstance()->GetTimerManager().ClearTimer(ReturnToMenuTimer);
	FWorldDelegates::OnWorldPostActorTick.RemoveAll(this);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnSessionUserInviteAcceptedDelegates.RemoveAll(this);
		SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
		SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
	}

	SessionSearch.Reset();
	DismissSessionInvite();
	SessionInterface.Reset();

	Super::Deinitialize();
}

FUniqueNetIdRepl UASSessionSubsystem::GetLocalUserId() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();
}

bool UASSessionSubsystem::IsSteamSession() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	return Subsystem && Subsystem->GetSubsystemName() == FName(TEXT("STEAM"));
}

bool UASSessionSubsystem::IsSessionServiceReady() const
{
	if (!SessionInterface.IsValid()) return false;
	if (!IsSteamSession()) return GIsEditor || FParse::Param(FCommandLine::Get(), TEXT("nosteam"));
	const IOnlineIdentityPtr Identity = Online::GetSubsystem(GetWorld())->GetIdentityInterface();
	return Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
}

FText UASSessionSubsystem::GetConnectionStatusText() const
{
	if (!IsSessionServiceReady()) return FText::FromString(TEXT("Steam 연결 실패: Steam에 로그인한 후 게임을 다시 실행해 주세요."));
	return FText::FromString(IsSteamSession() ? TEXT("Steam 온라인 · 친구 초대 사용 가능") : TEXT("LAN 테스트 모드 · Steam 초대 사용 불가"));
}

FOnlineSessionSettings UASSessionSubsystem::MakeSessionSettings(const bool bSteam, const int32 PublicConnections,
	const FString& RoomName, const bool bRequiresPassword)
{
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = FMath::Clamp(PublicConnections, 2, 250);
	Settings.bIsLANMatch = !bSteam;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = bSteam;
	Settings.bAllowJoinViaPresence = bSteam;
	Settings.bUsesPresence = bSteam;
	Settings.bUseLobbiesIfAvailable = bSteam;
	Settings.Set(RoomNameKey, RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(RequiresPasswordKey, bRequiresPassword, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GameIdKey, GameProtocolId, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(LobbyOpenKey, true, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	return Settings;
}

void UASSessionSubsystem::ConfigureSearch(FOnlineSessionSearch& Search, const bool bSteam, const int32 MaxResults)
{
	Search.bIsLanQuery = !bSteam;
	Search.MaxSearchResults = FMath::Clamp(MaxResults, 1, 1000);
	if (bSteam)
	{
		Search.QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
		Search.QuerySettings.Set(GameIdKey, GameProtocolId, EOnlineComparisonOp::Equals);
	}
}

bool UASSessionSubsystem::IsCompatibleSession(const FOnlineSessionSearchResult& Result)
{
	int32 Protocol = 0;
	bool bOpen = false;
	return Result.IsValid() && Result.Session.SessionSettings.Get(GameIdKey, Protocol) && Protocol == GameProtocolId
		&& Result.Session.SessionSettings.Get(LobbyOpenKey, bOpen) && bOpen;
}

bool UASSessionSubsystem::CanInviteFriends() const
{
	if (!IsSteamSession() || !IsSessionServiceReady() || bLeavingRoom || bDestroyingSession || bJoiningSession) return false;
	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	return Session && Session->SessionInfo.IsValid() && Session->SessionSettings.bAllowInvites
		&& Session->SessionSettings.bAllowJoinInProgress && Session->NumOpenPublicConnections > 0
		&& GetWorld() && GetWorld()->GetNetMode() != NM_Standalone;
}

bool UASSessionSubsystem::ShowInviteFriendsUI()
{
	if (!CanInviteFriends()) return false;
	const IOnlineExternalUIPtr ExternalUI = Online::GetSubsystem(GetWorld())->GetExternalUIInterface();
	return ExternalUI.IsValid() && ExternalUI->ShowInviteUI(0, NAME_GameSession);
}

void UASSessionSubsystem::HandleSessionInviteAccepted(const bool bWasSuccessful, const int32 ControllerId,
	FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	const FUniqueNetIdRepl LocalId = GetLocalUserId();
	if (!bWasSuccessful || !UserId.IsValid() || (LocalId.IsValid() && *LocalId != *UserId)
		|| !IsSteamSession() || !IsCompatibleSession(InviteResult))
	{
		ReturnToMenuMessage = FText::FromString(TEXT("초대한 방을 찾지 못했습니다. 같은 버전의 게임인지 확인하고 다시 초대해 주세요."));
		OnSessionJoined.Broadcast(false, ReturnToMenuMessage.ToString());
		return;
	}
	const FNamedOnlineSession* Current = SessionInterface->GetNamedSession(NAME_GameSession);
	if (Current && Current->SessionInfo.IsValid() && Current->GetSessionIdStr() == InviteResult.GetSessionIdStr()) return;
	DismissSessionInvite();
	PendingInvite = InviteResult;
	// World tick waits for in-flight operations and startup travel before switching rooms.
}

void UASSessionSubsystem::ProcessPendingInvite(UWorld* World)
{
	if (!PendingInvite.IsSet() || bLeavingRoom || bDestroyingSession || bCreatingSession || bJoiningSession || bFindingSessions
		|| !World->GetFirstPlayerController()) return;
	if (SessionInterface.IsValid() && (SessionInterface->GetNamedSession(NAME_GameSession) || World->GetNetMode() != NM_Standalone))
	{
		ReturnToMenuWithError(FText::FromString(TEXT("친구 초대를 수락하여 기존 방에서 나왔습니다.")));
		return;
	}
	if (bInviteAccepted)
	{
		const FOnlineSessionSearchResult Result = PendingInvite.GetValue();
		const FString Password = AcceptedInvitePassword;
		DismissSessionInvite();
		JoinSessionResult(Result, Password);
		return;
	}
	if (InviteWidget && InviteWidget->IsInViewport()) return;
	InviteWidget = CreateWidget<UASSessionInviteWidget>(World->GetFirstPlayerController());
	if (!InviteWidget) return;
	FString RoomName = PendingInvite->Session.OwningUserName;
	bool bPassword = false;
	PendingInvite->Session.SessionSettings.Get(RoomNameKey, RoomName);
	PendingInvite->Session.SessionSettings.Get(RequiresPasswordKey, bPassword);
	InviteWidget->Configure(RoomName, bPassword);
	InviteWidget->AddToViewport(300);
	InviteWidget->SetKeyboardFocus();
}

bool UASSessionSubsystem::AcceptSessionInvite(const FString& Password)
{
	if (!PendingInvite.IsSet()) return false;
	bool bPassword = false;
	PendingInvite->Session.SessionSettings.Get(RequiresPasswordKey, bPassword);
	if (bPassword && !IsValidRoomPassword(Password)) return false;
	AcceptedInvitePassword = bPassword ? Password : FString();
	bInviteAccepted = true;
	return true;
}

void UASSessionSubsystem::DismissSessionInvite()
{
	PendingInvite.Reset();
	AcceptedInvitePassword.Reset();
	bInviteAccepted = false;
	if (InviteWidget) InviteWidget->RemoveFromParent();
	InviteWidget = nullptr;
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
	if (bLeavingRoom || bDestroyingSession || bJoiningSession || bCreatingSession || bFindingSessions)
	{
		OnSessionCreated.Broadcast(false);
		return;
	}
	if (bUsePassword && !IsValidRoomPassword(Password))
	{
		OnSessionCreated.Broadcast(false);
		return;
	}
	if (!IsSessionServiceReady())
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

	const FOnlineSessionSettings SessionSettings = MakeSessionSettings(IsSteamSession(), PublicConnections, RoomName, bUsePassword);
	HostPassword = bUsePassword ? Password : FString();
	ReturnToMenuMessage = FText::GetEmpty();
	ClearMatchCharacters();

	SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(
		this,
		&ThisClass::HandleCreateSessionComplete);

	bCreatingSession = true;
	if (!SessionInterface->CreateSession(*UserId, NAME_GameSession, SessionSettings))
	{
		SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
		bCreatingSession = false;
		OnSessionCreated.Broadcast(false);
	}
}

void UASSessionSubsystem::FindSessions(const int32 MaxResults)
{
	if (!IsSessionServiceReady() || bLeavingRoom || bDestroyingSession || bJoiningSession || bCreatingSession || bFindingSessions)
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
	ConfigureSearch(*SessionSearch, IsSteamSession(), MaxResults);

	SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
	SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(
		this,
		&ThisClass::HandleFindSessionsComplete);

	bFindingSessions = true;
	if (!SessionInterface->FindSessions(*UserId, SessionSearch.ToSharedRef()))
	{
		SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
		SessionSearch.Reset();
		bFindingSessions = false;
		OnSessionsFound.Broadcast(false, TArray<FASSessionSearchResult>());
	}
}

void UASSessionSubsystem::JoinSessionByIndex(const int32 ResultIndex, const FString& Password)
{
	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(ResultIndex))
	{
		OnSessionJoined.Broadcast(false, TEXT("참가할 세션을 찾지 못했습니다. 방을 다시 검색해 주세요."));
		return;
	}
	JoinSessionResult(SessionSearch->SearchResults[ResultIndex], Password);
}

void UASSessionSubsystem::JoinSessionResult(const FOnlineSessionSearchResult& SearchResult, const FString& Password)
{
	if (bLeavingRoom || bDestroyingSession || bJoiningSession || bCreatingSession || bFindingSessions)
	{
		OnSessionJoined.Broadcast(false, TEXT("이전 세션 요청을 처리 중입니다. 잠시 후 다시 시도해 주세요."));
		return;
	}
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
	{
		OnSessionJoined.Broadcast(false, TEXT("이미 세션에 참가해 있습니다."));
		return;
	}
	if (!IsSessionServiceReady() || !IsCompatibleSession(SearchResult))
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
	SearchResult.Session.SessionSettings.Get(RequiresPasswordKey, bRequiresPassword);
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

	bJoiningSession = true;
	GetGameInstance()->GetTimerManager().SetTimer(JoinTimeoutTimer, this, &ThisClass::HandleJoinTimeout, 30.0f, false);
	GetGameInstance()->GetSubsystem<UASLoadingScreenSubsystem>()->ShowLoadingScreen(FSimpleDelegate::CreateUObject(this, &ThisClass::CancelJoin));
	if (!SessionInterface->JoinSession(
		*UserId,
		NAME_GameSession,
		SearchResult))
	{
		StopJoinAttempt();
		DestroySession();
		OnSessionJoined.Broadcast(false, TEXT("세션 참가 요청을 시작하지 못했습니다."));
	}
}

void UASSessionSubsystem::DestroySession()
{
	if (bDestroyingSession) return;
	bDestroyingSession = true;
	if (!SessionInterface.IsValid())
	{
		HandleDestroySessionComplete(NAME_GameSession, false);
		return;
	}
	if (!SessionInterface->GetNamedSession(NAME_GameSession))
	{
		HandleDestroySessionComplete(NAME_GameSession, true);
		return;
	}
	SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
	SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &ThisClass::HandleDestroySessionComplete);
	GetGameInstance()->GetTimerManager().SetTimer(DestroyTimeoutTimer, this, &ThisClass::HandleDestroyTimeout, 5.0f, false);
	if (!SessionInterface->DestroySession(NAME_GameSession)) HandleDestroySessionComplete(NAME_GameSession, false);
}

void UASSessionSubsystem::StopJoinAttempt()
{
	bJoiningSession = false;
	bJoinTravelPending = false;
	PendingJoinPassword.Reset();
	GetGameInstance()->GetTimerManager().ClearTimer(JoinTimeoutTimer);
	if (SessionInterface.IsValid()) SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);
	GetGameInstance()->GetSubsystem<UASLoadingScreenSubsystem>()->HideLoadingScreen();
}

void UASSessionSubsystem::LeaveRoom()
{
	ReturnToMenuWithError(FText::GetEmpty());
}

void UASSessionSubsystem::CancelJoin()
{
	if (bJoiningSession) ReturnToMenuWithError(FText::FromString(TEXT("방 참가를 취소했습니다.")));
}

void UASSessionSubsystem::HandleJoinTimeout()
{
	if (bJoiningSession) ReturnToMenuWithError(FText::FromString(TEXT("30초 안에 서버에 연결하지 못했습니다. 다시 검색하여 참가해 주세요.")));
}

void UASSessionSubsystem::HandleDestroyTimeout()
{
	UE_LOG(LogTemp, Warning, TEXT("[Session] Session cleanup timed out; removing the local session."));
	HandleDestroySessionComplete(NAME_GameSession, false);
}

void UASSessionSubsystem::ReturnToMenuWithError(const FText& Message)
{
	if (bLeavingRoom || !GetWorld()) return;
	bLeavingRoom = true;
	ReturnToMenuMessage = Message;
	const bool bWasJoining = bJoiningSession;
	StopJoinAttempt();
	if (bWasJoining && GEngine)
	{
		GEngine->CancelPending(GetWorld());
		// ClientTravel may still be queued and have no PendingNetGame to cancel yet.
		GEngine->SetClientTravel(GetWorld(), TEXT(""), TRAVEL_Absolute);
	}
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
		if (bFindingSessions) SessionInterface->CancelFindSessions();
	}
	bCreatingSession = false;
	bFindingSessions = false;
	if (GetWorld()->GetNetMode() == NM_ListenServer) SetLobbyOpen(false);
	DestroySession();
}

void UASSessionSubsystem::FinishLeavingRoom()
{
	if (!bLeavingRoom) return;
	ClearMatchCharacters();
	SessionSearch.Reset();
	HostPassword.Reset();
	PendingJoinPassword.Reset();
	if (GetGameInstance()->GetTimerManager().TimerExists(ReturnToMenuTimer)) return;
	// Leave the current engine/network callback before changing worlds.
	ReturnToMenuTimer = GetGameInstance()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
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
	Settings.bAllowInvites = bOpen && IsSteamSession();
	Settings.bAllowJoinViaPresence = bOpen && IsSteamSession();
	Settings.Set(LobbyOpenKey, bOpen, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
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
	if (!bCreatingSession || bLeavingRoom) return;
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
	}

	bCreatingSession = false;
	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 생성: %s"), bWasSuccessful ? TEXT("성공") : TEXT("실패"));
	OnSessionCreated.Broadcast(bWasSuccessful);
}

void UASSessionSubsystem::HandleFindSessionsComplete(const bool bWasSuccessful)
{
	if (!bFindingSessions || bLeavingRoom) return;
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
	}

	bFindingSessions = false;
	TArray<FASSessionSearchResult> Results;
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& SearchResult = SessionSearch->SearchResults[Index];
			if (!IsCompatibleSession(SearchResult)) continue;
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
	if (!bJoiningSession || bLeavingRoom) return;
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);
	}

	if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface.IsValid())
	{
		StopJoinAttempt();
		DestroySession();
		OnSessionJoined.Broadcast(false, TEXT("세션 참가에 실패했습니다."));
		return;
	}

	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		StopJoinAttempt();
		DestroySession();
		OnSessionJoined.Broadcast(false, TEXT("호스트 주소를 확인하지 못했습니다."));
		return;
	}

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		StopJoinAttempt();
		DestroySession();
		OnSessionJoined.Broadcast(false, TEXT("플레이어 컨트롤러를 찾지 못했습니다."));
		return;
	}

	bJoinTravelPending = true;
	if (!PendingJoinPassword.IsEmpty()) ConnectString += TEXT("?RoomPassword=") + PendingJoinPassword;
	PendingJoinPassword.Reset();
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	OnSessionJoined.Broadcast(true, FString());
}

void UASSessionSubsystem::HandleDestroySessionComplete(
	const FName SessionName,
	const bool bWasSuccessful)
{
	if (!bDestroyingSession) return;
	bDestroyingSession = false;
	GetGameInstance()->GetTimerManager().ClearTimer(DestroyTimeoutTimer);
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
		if (!bWasSuccessful) SessionInterface->RemoveNamedSession(NAME_GameSession);
	}
	HostPassword.Reset();
	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 종료: %s"), bWasSuccessful ? TEXT("성공") : TEXT("실패"));
	OnSessionDestroyed.Broadcast(bWasSuccessful);
	FinishLeavingRoom();
}

bool UASSessionSubsystem::ShouldEndHostSessionForNetworkFailure(const ENetworkFailure::Type FailureType)
{
	return FailureType == ENetworkFailure::NetDriverAlreadyExists || FailureType == ENetworkFailure::NetDriverCreateFailure
		|| FailureType == ENetworkFailure::NetDriverListenFailure;
}

void UASSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
	if (!World || World->GetGameInstance() != GetGameInstance()) return;
	if (NetDriver && NetDriver != World->GetNetDriver() && NetDriver->NetDriverName != FName(TEXT("PendingNetDriver"))
		&& NetDriver->NetDriverName != FName(TEXT("GameNetDriver"))) return;
	// A remote player's connection failure is handled by Logout, not by closing the host's room.
	if (World->GetNetMode() == NM_ListenServer && !ShouldEndHostSessionForNetworkFailure(FailureType)) return;
	HandleConnectionFailure(World, Error.IsEmpty() ? TEXT("서버 연결에 실패했습니다.") : Error);
}

void UASSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (!World || World->GetGameInstance() != GetGameInstance()) return;
	UE_LOG(LogTemp, Warning, TEXT("[Session] Travel failed: %s"), *Error);
	ReturnToMenuWithError(FText::FromString(TEXT("맵 이동에 실패하여 메인메뉴로 돌아왔습니다. 다시 방을 생성하거나 참가해 주세요.")));
}

void UASSessionSubsystem::HandleMapLoaded(UWorld* World)
{
	if (World && World->GetGameInstance() == GetGameInstance() && World->GetNetMode() == NM_Standalone)
	{
		bLeavingRoom = false;
	}
}

void UASSessionSubsystem::HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World && World == GetWorld() && World->HasBegunPlay()) ProcessPendingInvite(World);
	if (bJoiningSession && bJoinTravelPending && World && World == GetWorld() && World->GetNetMode() == NM_Client
		&& World->HasBegunPlay() && World->GetFirstPlayerController())
	{
		StopJoinAttempt();
		UE_LOG(LogTemp, Log, TEXT("[Session] Joined world is ready."));
	}
}

void UASSessionSubsystem::HandleConnectionFailure(UWorld* World, const FString& Error)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || bLeavingRoom) return;
	UE_LOG(LogTemp, Warning, TEXT("[Session] %s"), *Error);
	const bool bWasJoining = bJoiningSession;
	const FText Message = FText::FromString(bWasJoining && Error.Contains(TEXT("AS_WRONG_PASSWORD"))
		? TEXT("비밀번호가 일치하지 않습니다. 방을 다시 선택해 주세요.")
		: Error.Contains(TEXT("AS_GAME_IN_PROGRESS")) || Error.Contains(TEXT("Game is starting."))
			? TEXT("이미 시작한 방에는 참가할 수 없습니다. 다른 방을 선택해 주세요.")
			: World->GetNetMode() == NM_ListenServer ? TEXT("서버 오류로 방을 종료했습니다. 다시 방을 생성해 주세요.")
			: TEXT("서버와 연결이 종료되었습니다. 방장이 나갔거나 연결이 끊겼습니다."));
	ReturnToMenuWithError(Message);
	if (bWasJoining) OnSessionJoined.Broadcast(false, Error);
}
