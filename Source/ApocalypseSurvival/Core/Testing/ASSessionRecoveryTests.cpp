#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Characters/Player/Data/ASCharacterRoster.h"
#include "Core/Framework/ASLobbyGameMode.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASHostFailurePolicyTest,
	"ApocalypseSurvival.Session.HostFailurePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASHostFailurePolicyTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("One client disconnecting must not close the host's room"), UASSessionSubsystem::ShouldEndHostSessionForNetworkFailure(ENetworkFailure::ConnectionLost));
	TestFalse(TEXT("One client timing out must not close the host's room"), UASSessionSubsystem::ShouldEndHostSessionForNetworkFailure(ENetworkFailure::ConnectionTimeout));
	TestFalse(TEXT("A rejected connection must not close the host's room"), UASSessionSubsystem::ShouldEndHostSessionForNetworkFailure(ENetworkFailure::FailureReceived));
	TestTrue(TEXT("A failed listener requires host recovery"), UASSessionSubsystem::ShouldEndHostSessionForNetworkFailure(ENetworkFailure::NetDriverListenFailure));
	TestTrue(TEXT("A failed driver creation requires host recovery"), UASSessionSubsystem::ShouldEndHostSessionForNetworkFailure(ENetworkFailure::NetDriverCreateFailure));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASSelectionFallbackTest,
	"ApocalypseSurvival.CharacterSelection.InvalidSelectionFallsBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASSelectionFallbackTest::RunTest(const FString& Parameters)
{
	UASCharacterRoster* Roster = NewObject<UASCharacterRoster>();
	UASCharacterDefinition* Invalid = NewObject<UASCharacterDefinition>();
	UASCharacterDefinition* First = NewObject<UASCharacterDefinition>();
	UASCharacterDefinition* Second = NewObject<UASCharacterDefinition>();
	First->CharacterClass = AASPlayerCharacter::StaticClass();
	Second->CharacterClass = AASPlayerCharacter::StaticClass();
	Roster->Characters = { nullptr, Invalid, First, Second };
	TestEqual(TEXT("A valid last choice is preserved"), AASLobbyGameMode::ResolveCharacterIndex(Roster, 3), 3);
	TestEqual(TEXT("Late join with no selection gets the first playable character"), AASLobbyGameMode::ResolveCharacterIndex(Roster, INDEX_NONE), 2);
	TestEqual(TEXT("An entry without a pawn class cannot block the deadline"), AASLobbyGameMode::ResolveCharacterIndex(Roster, 1), 2);
	TestEqual(TEXT("An out of range choice gets a safe default"), AASLobbyGameMode::ResolveCharacterIndex(Roster, 99), 2);
	Roster->Characters = { nullptr, Invalid };
	TestEqual(TEXT("No playable characters reports failure instead of a bad index"), AASLobbyGameMode::ResolveCharacterIndex(Roster, 0), INDEX_NONE);
	TestEqual(TEXT("Missing roster reports failure"), AASLobbyGameMode::ResolveCharacterIndex(nullptr, 0), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASSessionCleanupTest,
	"ApocalypseSurvival.Session.FailedCleanupAllowsRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASSessionCleanupTest::RunTest(const FString& Parameters)
{
	IOnlineSubsystem* Online = IOnlineSubsystem::Get(FName(TEXT("NULL")));
	IOnlineSessionPtr Interface = Online ? Online->GetSessionInterface() : nullptr;
	if (!TestTrue(TEXT("Null session interface"), Interface.IsValid())) return false;
	if (!TestNull(TEXT("The test must not replace a real session"), Interface->GetNamedSession(NAME_GameSession))) return false;
	UGameInstance* Instance = NewObject<UGameInstance>();
	UASSessionSubsystem* Sessions = NewObject<UASSessionSubsystem>(Instance);
	Sessions->SessionInterface = Interface;
	FOnlineSessionSettings Settings;
	Settings.bShouldAdvertise = false;
	Settings.bIsLANMatch = true;
	Settings.NumPublicConnections = 2;
	TestTrue(TEXT("Create a local test session"), Interface->CreateSession(0, NAME_GameSession, Settings));
	Sessions->bDestroyingSession = true;
	Sessions->HandleDestroySessionComplete(NAME_GameSession, false);
	TestNull(TEXT("Failed cleanup removes the stale local session even without LeaveRoom"), Interface->GetNamedSession(NAME_GameSession));
	TestFalse(TEXT("Failed cleanup releases the operation lock"), Sessions->bDestroyingSession);
	TestTrue(TEXT("A new session can be created after failed cleanup"), Interface->CreateSession(0, NAME_GameSession, Settings));
	Sessions->bDestroyingSession = true;
	Sessions->HandleDestroyTimeout();
	TestNull(TEXT("Cleanup timeout also removes the stale local session"), Interface->GetNamedSession(NAME_GameSession));
	TestFalse(TEXT("Cleanup timeout releases the operation lock"), Sessions->bDestroyingSession);
	Sessions->DestroySession();
	TestFalse(TEXT("Repeated cleanup without a session finishes immediately"), Sessions->bDestroyingSession);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASJoinCancellationTest,
	"ApocalypseSurvival.Session.CancelAndTimeoutClearQueuedTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASJoinCancellationTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(FName(TEXT("SessionCancellationTest")));
	UWorld* World = Instance->GetWorld();
	UASSessionSubsystem* Sessions = Instance->GetSubsystem<UASSessionSubsystem>();
	if (!TestNotNull(TEXT("Standalone test world"), World) || !TestNotNull(TEXT("Initialized sessions"), Sessions))
	{
		Instance->Shutdown();
		return false;
	}
	FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
	GEngine->SetClientTravel(World, TEXT("127.0.0.1"), TRAVEL_Absolute);
	Sessions->bJoiningSession = true;
	Sessions->CancelJoin();
	TestFalse(TEXT("Cancellation ends the join attempt"), Sessions->bJoiningSession);
	TestTrue(TEXT("Cancellation enters menu recovery"), Sessions->IsLeavingRoom());
	TestTrue(TEXT("Cancellation clears even a not-yet-started ClientTravel"), Context->TravelURL.IsEmpty());
	Instance->GetTimerManager().ClearTimer(Sessions->ReturnToMenuTimer);
	Sessions->bLeavingRoom = false;
	Sessions->bJoiningSession = true;
	GEngine->SetClientTravel(World, TEXT("127.0.0.1"), TRAVEL_Absolute);
	Sessions->HandleJoinTimeout();
	TestFalse(TEXT("Timeout ends the join attempt"), Sessions->bJoiningSession);
	TestTrue(TEXT("Timeout clears queued ClientTravel"), Context->TravelURL.IsEmpty());
	TestTrue(TEXT("Timeout preserves an explanation for the menu"), Sessions->ReturnToMenuMessage.ToString().Contains(TEXT("30")));
	Instance->GetTimerManager().ClearTimer(Sessions->ReturnToMenuTimer);
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
