#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystemTypes.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Core/UI/ASJoinRoomWidget.h"
#include "Core/UI/ASLobbyWidget.h"

namespace
{
	class FASTestSessionInfo final : public FOnlineSessionInfo
	{
	public:
		virtual const uint8* GetBytes() const override { return Id->GetBytes(); }
		virtual int32 GetSize() const override { return Id->GetSize(); }
		virtual bool IsValid() const override { return true; }
		virtual FString ToString() const override { return Id->ToString(); }
		virtual FString ToDebugString() const override { return ToString(); }
		virtual const FUniqueNetId& GetSessionId() const override { return *Id; }
	private:
		FUniqueNetIdStringRef Id = FUniqueNetIdString::Create(TEXT("TestLobby"), FName(TEXT("NULL")));
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASSteamSessionSettingsTest,
	"ApocalypseSurvival.Session.SteamLobbySettingsAndSearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASSteamSessionSettingsTest::RunTest(const FString& Parameters)
{
	const FOnlineSessionSettings Steam = UASSessionSubsystem::MakeSessionSettings(true, 4, TEXT("Test Room"), TEXT("123456"), true);
	TestFalse(TEXT("Steam sessions must use internet matchmaking"), Steam.bIsLANMatch);
	TestFalse(TEXT("Host is a listen server"), Steam.bIsDedicated);
	TestTrue(TEXT("Steam presence and lobbies must both be enabled"), Steam.bUsesPresence && Steam.bUseLobbiesIfAvailable);
	TestTrue(TEXT("Invite and friend join are available in the lobby"), Steam.bAllowInvites && Steam.bAllowJoinViaPresence);
	TestEqual(TEXT("Player limit is retained"), Steam.NumPublicConnections, 4);
	bool bPassword = false;
	TestTrue(TEXT("Password requirement is advertised"), Steam.Get(FName(TEXT("RequiresPassword")), bPassword) && bPassword);
	TestFalse(TEXT("Actual password is never advertised"), Steam.Settings.Contains(FName(TEXT("RoomPassword"))));
	const FOnlineSessionSettings LAN = UASSessionSubsystem::MakeSessionSettings(false, 4, TEXT("LAN"), TEXT("123456"), false);
	TestTrue(TEXT("LAN fallback remains LAN"), LAN.bIsLANMatch);
	TestFalse(TEXT("LAN does not request Steam lobbies or invites"), LAN.bUseLobbiesIfAvailable || LAN.bUsesPresence || LAN.bAllowInvites);
	FOnlineSessionSearch Search;
	UASSessionSubsystem::ConfigureSearch(Search, true, 100);
	bool bLobbies = false;
	TestFalse(TEXT("Steam search is not a LAN broadcast"), Search.bIsLanQuery);
	TestTrue(TEXT("Steam search explicitly requests lobbies"), Search.QuerySettings.Get(SEARCH_LOBBIES, bLobbies) && bLobbies);
	int32 QueryProtocol = 0;
	int32 AdvertisedProtocol = 0;
	TestTrue(TEXT("Search includes our game protocol filter"), Search.QuerySettings.Get(FName(TEXT("ASGame")), QueryProtocol));
	Steam.Get(FName(TEXT("ASGame")), AdvertisedProtocol);
	TestEqual(TEXT("Creation and search use the same protocol"), QueryProtocol, AdvertisedProtocol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASSteamInviteValidationTest,
	"ApocalypseSurvival.Session.SteamInviteValidationAndPassword",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASSteamInviteValidationTest::RunTest(const FString& Parameters)
{
	FOnlineSessionSearchResult Result;
	Result.Session.OwningUserId = FUniqueNetIdString::Create(TEXT("TestHost"), FName(TEXT("NULL")));
	Result.Session.SessionInfo = MakeShared<FASTestSessionInfo>();
	Result.Session.SessionSettings = UASSessionSubsystem::MakeSessionSettings(true, 4, TEXT("Invite Room"), TEXT("123456"), true);
	TestTrue(TEXT("Our open room is compatible"), UASSessionSubsystem::IsCompatibleSession(Result));
	Result.Session.SessionSettings.Set(FName(TEXT("LobbyOpen")), false);
	TestFalse(TEXT("A game already starting cannot be joined by invite"), UASSessionSubsystem::IsCompatibleSession(Result));
	Result.Session.SessionSettings.Set(FName(TEXT("LobbyOpen")), true);
	Result.Session.SessionSettings.Set(FName(TEXT("ASGame")), 123);
	TestFalse(TEXT("Other games on App ID 480 are rejected"), UASSessionSubsystem::IsCompatibleSession(Result));
	Result.Session.SessionSettings = UASSessionSubsystem::MakeSessionSettings(true, 4, TEXT("Invite Room"), TEXT("123456"), true);
	UGameInstance* Instance = NewObject<UGameInstance>();
	UASSessionSubsystem* Sessions = NewObject<UASSessionSubsystem>(Instance);
	Sessions->PendingInvite = Result;
	TestFalse(TEXT("Invite cannot bypass password entry"), Sessions->AcceptSessionInvite(TEXT("")));
	TestFalse(TEXT("Non-digit password is rejected"), Sessions->AcceptSessionInvite(TEXT("abcd")));
	TestTrue(TEXT("Valid-format password queues join for server validation"), Sessions->AcceptSessionInvite(TEXT("0123")));
	TestTrue(TEXT("Invite waits until world tick"), Sessions->bInviteAccepted && Sessions->PendingInvite.IsSet());
	Sessions->bJoiningSession = true;
	Sessions->ProcessPendingInvite(nullptr);
	TestTrue(TEXT("Another join finishing cannot consume the invite"), Sessions->PendingInvite.IsSet());
	Sessions->DismissSessionInvite();
	TestFalse(TEXT("Dismiss releases the pending invite"), Sessions->PendingInvite.IsSet() || Sessions->bInviteAccepted);
	TestTrue(TEXT("Dismiss clears the password"), Sessions->AcceptedInvitePassword.IsEmpty());
	TestFalse(TEXT("A dismissed invite cannot be joined"), Sessions->AcceptSessionInvite(TEXT("0123")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASRoomCodeTest,
	"ApocalypseSurvival.Session.RoomCode.ValidationAndRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASRoomCodeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Leading zeros are retained"), UASSessionSubsystem::IsValidRoomCode(TEXT("001234")));
	for (const FString Code : { TEXT(""), TEXT("12345"), TEXT("1234567"), TEXT("-12345"), TEXT("123.45"), TEXT("12A456") })
	{
		TestFalse(TEXT("Only six ASCII digits are accepted"), UASSessionSubsystem::IsValidRoomCode(Code));
	}
	UGameInstance* Instance = NewObject<UGameInstance>();
	UASSessionSubsystem* Sessions = NewObject<UASSessionSubsystem>(Instance);
	for (const bool bSearchSucceeded : { false, true })
	{
		Sessions->bFindingSessions = true;
		Sessions->PendingRoomCode = TEXT("001234");
		Sessions->PendingRoomCodePassword = TEXT("0123");
		Sessions->SessionSearch = MakeShared<FOnlineSessionSearch>();
		Sessions->HandleFindSessionsComplete(bSearchSucceeded);
		TestFalse(TEXT("Failed/empty search releases the request lock"), Sessions->bFindingSessions);
		TestTrue(TEXT("Failed/empty search cannot leak a code or password into a retry"),
			Sessions->PendingRoomCode.IsEmpty() && Sessions->PendingRoomCodePassword.IsEmpty());
	}
	FOnlineSessionSearchResult Result;
	Result.Session.OwningUserId = FUniqueNetIdString::Create(TEXT("Host"), FName(TEXT("NULL")));
	Result.Session.SessionInfo = MakeShared<FASTestSessionInfo>();
	Result.Session.SessionSettings = UASSessionSubsystem::MakeSessionSettings(false, 4, TEXT("Room"), TEXT("001234"), false);
	FString AdvertisedCode;
	TestTrue(TEXT("Code is discoverable without changing leading zeros"),
		Result.Session.SessionSettings.Get(FName(TEXT("RoomCode")), AdvertisedCode) && AdvertisedCode == TEXT("001234"));
	Result.Session.NumOpenPublicConnections = 4;
	Sessions->SessionSearch->SearchResults = { Result, Result };
	Sessions->PendingRoomCode = TEXT("001234");
	Sessions->bFindingSessions = true;
	Sessions->HandleFindSessionsComplete(true);
	TestFalse(TEXT("Duplicate codes never arbitrarily join one host"), Sessions->bJoiningSession);
	TestTrue(TEXT("Duplicate-code error allows retry"), Sessions->PendingRoomCode.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASRoomCodeWidgetsTest,
	"ApocalypseSurvival.Session.RoomCode.ExistingScreensSupportControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASRoomCodeWidgetsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Path : {
		TEXT("/Game/ApocalypseSurvival/Core/UI/MainMenu/WBP_JoinRoomPanel.WBP_JoinRoomPanel_C"),
		TEXT("/Game/ApocalypseSurvival/Core/UI/MainMenu/WBP_Lobby.WBP_Lobby_C") })
	{
		UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UUserWidget>(nullptr, Path));
		if (!TestNotNull(TEXT("Existing lobby/menu widget loads"), WidgetClass)) continue;
		UWidgetBlueprintGeneratedClass* Owner = WidgetClass->FindWidgetTreeOwningClass();
		UWidgetTree* Tree = Owner ? Owner->GetWidgetTreeArchetype() : nullptr;
		TestTrue(TEXT("Existing screen supports automatic room-code controls"), Tree && Cast<UCanvasPanel>(Tree->RootWidget));
	}
	return true;
}

#endif
