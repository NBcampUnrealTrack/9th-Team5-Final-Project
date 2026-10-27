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
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GAS/Abilities/ASGameplayAbility_CharacterPrimaryAttack.h"
#include "GAS/ASAbilityTags.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASNewMatchSelectionTest,
	"ApocalypseSurvival.CharacterSelection.NewMatchClearsPreviousCharacter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASNewMatchSelectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AASPlayerState* State = World->SpawnActor<AASPlayerState>();
	if (!TestNotNull(TEXT("Player state"), State))
	{
		World->DestroyWorld(false);
		return false;
	}
	UASAbilitySystemComponent* ASC = Cast<UASAbilitySystemComponent>(State->GetAbilitySystemComponent());
	ASC->InitAbilityActorInfo(State, State);
	ASC->AddAttributeSetSubobject(NewObject<UASCombatAttributeSet>(State));
	const UASCharacterDefinition* First = NewObject<UASCharacterDefinition>();
	const UASCharacterDefinition* Second = NewObject<UASCharacterDefinition>();
	State->SetCharacterDefinition(First);
	State->ApplyCharacterDefinitionOnce();
	State->MarkCharacterSelectionReceived();
	State->SetCharacterDefinition(Second);
	TestTrue(TEXT("Character remains locked during the current match"), State->GetCharacterDefinition() == First);
	const FGameplayAbilitySpecHandle OldAbility = ASC->GiveAbility(FGameplayAbilitySpec(UASGameplayAbility_CharacterPrimaryAttack::StaticClass()));
	ASC->SetNumericAttributeBase(UASCombatAttributeSet::GetAttackPowerAttribute(), 999.0f);
	ASC->AddLooseGameplayTag(ASGameplayTags::State_Downed);
	State->ResetForNewMatch();
	TestNull(TEXT("Previous match ability is removed"), ASC->FindAbilitySpecFromHandle(OldAbility));
	TestFalse(TEXT("Previous downed state is removed"), ASC->HasMatchingGameplayTag(ASGameplayTags::State_Downed));
	TestEqual(TEXT("Previous attack growth is reset"), ASC->GetNumericAttributeBase(UASCombatAttributeSet::GetAttackPowerAttribute()),
		GetDefault<UASCombatAttributeSet>()->GetAttackPower());
	TestNull(TEXT("Lobby return clears the previous character"), State->GetCharacterDefinition());
	TestFalse(TEXT("New match requires a fresh selection"), State->HasReceivedCharacterSelection());
	State->SetCharacterDefinition(Second);
	State->ApplyCharacterDefinitionOnce();
	TestTrue(TEXT("Second match applies the newly selected character"), State->GetCharacterDefinition() == Second);
	World->DestroyWorld(false);
	return true;
}

#endif
