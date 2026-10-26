#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Characters/Player/Data/ASCharacterRoster.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "Core/Framework/ASPlayerState.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OnlineSubsystemTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASMatchCharacterCacheTest,
	"ApocalypseSurvival.CharacterSelection.PlayerChoicesSurviveTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASMatchCharacterCacheTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	UASSessionSubsystem* Sessions = NewObject<UASSessionSubsystem>(Instance);
	const FUniqueNetIdRef FirstNetId = FUniqueNetIdString::Create(TEXT("FirstPlayer"), TEXT("NULL"));
	const FUniqueNetIdRef SecondNetId = FUniqueNetIdString::Create(TEXT("SecondPlayer"), TEXT("NULL"));
	const FUniqueNetIdRepl FirstId(FirstNetId);
	const FUniqueNetIdRepl SecondId(SecondNetId);
	const UASCharacterDefinition* First = NewObject<UASCharacterDefinition>();
	const UASCharacterDefinition* Second = NewObject<UASCharacterDefinition>();
	Sessions->SaveMatchCharacter(FirstId, First);
	Sessions->SaveMatchCharacter(SecondId, Second);
	TestTrue(TEXT("First player's choice is isolated"), Sessions->FindMatchCharacter(FirstId) == First);
	TestTrue(TEXT("Second player's choice is isolated"), Sessions->FindMatchCharacter(SecondId) == Second);
	Sessions->SaveMatchCharacter(FirstId, Second);
	TestTrue(TEXT("Last server choice wins"), Sessions->FindMatchCharacter(FirstId) == Second);
	TestNull(TEXT("Unknown identity cannot use another player's choice"), Sessions->FindMatchCharacter(FUniqueNetIdRepl()));
	Sessions->ClearMatchCharacters();
	TestNull(TEXT("New match clears old choices"), Sessions->FindMatchCharacter(FirstId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASCharacterSelectionStateTest,
	"ApocalypseSurvival.CharacterSelection.DeadlineAndPlayerStateCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASCharacterSelectionStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AASLobbyGameState* State = World->SpawnActor<AASLobbyGameState>();
	AASPlayerState* Source = World->SpawnActor<AASPlayerState>();
	AASPlayerState* Destination = World->SpawnActor<AASPlayerState>();
	if (!State || !Source || !Destination)
	{
		AddError(TEXT("Could not create test actors"));
		World->DestroyWorld(false);
		return false;
	}
	World->SetGameState(State);
	UASCharacterRoster* Roster = NewObject<UASCharacterRoster>();
	State->BeginCharacterSelection(Roster, 30.0f);
	TestTrue(TEXT("Selection phase started"), State->LobbyPhase == EASLobbyPhase::CharacterSelection);
	TestEqual(TEXT("Shared deadline starts at 30 seconds"), State->GetCharacterSelectionTimeRemaining(), 30.0f);
	State->CharacterSelectionEndTime = State->GetServerWorldTimeSeconds() - 1.0f;
	TestEqual(TEXT("Expired deadline cannot become negative"), State->GetCharacterSelectionTimeRemaining(), 0.0f);
	const UASCharacterDefinition* Definition = NewObject<UASCharacterDefinition>();
	Source->SetCharacterDefinition(Definition);
	Source->MarkCharacterSelectionReceived();
	Source->CopyProperties(Destination);
	TestTrue(TEXT("Seamless travel copies the selected character"), Destination->GetCharacterDefinition() == Definition);
	TestTrue(TEXT("Copied selection is ready before spawning"), Destination->HasReceivedCharacterSelection());
	State->MarkTravelling();
	TestTrue(TEXT("Selection closes when travelling"), State->LobbyPhase == EASLobbyPhase::Travelling);
	World->DestroyWorld(false);
	return true;
}

#endif
