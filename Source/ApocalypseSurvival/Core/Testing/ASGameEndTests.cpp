#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/Framework/ASGameState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Items/Combat/ASWeaponProjectile.h"
#include "MassSimulationSubsystem.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASGameEndStopsGameplayTest,
	"ApocalypseSurvival.GameFlow.EndStopsGameplayWithoutWorldPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASGameEndStopsGameplayTest::RunTest(const FString& Parameters)
{
	for (const EGamePhase Result : { EGamePhase::Victory, EGamePhase::Defeat })
	{
		UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
		Instance->InitializeStandalone(FName(TEXT("GameEndTest")));
		UWorld* World = Instance->GetWorld();
		World->InitializeActorsForPlay(FURL());
		AASGameState* State = World->SpawnActor<AASGameState>();
		World->SetGameState(State);
		ACharacter* Character = World->SpawnActor<ACharacter>();
		APlayerController* Controller = World->SpawnActor<APlayerController>();
		Controller->Possess(Character);
		AASWeaponProjectile* Projectile = World->SpawnActor<AASWeaponProjectile>();
		World->GetWorldSettings()->NotifyBeginPlay();
		Character->GetCharacterMovement()->Velocity = FVector(200.0f, 0.0f, 0.0f);

		UASAbilitySystemComponent* Abilities = NewObject<UASAbilitySystemComponent>(Character);
		Abilities->RegisterComponent();
		Abilities->InitAbilityActorInfo(Character, Character);
		UASHealthAttributeSet* Health = NewObject<UASHealthAttributeSet>(Character);
		Abilities->AddAttributeSetSubobject(Health);
		UGameplayEffect* Damage = NewObject<UGameplayEffect>();
		Damage->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Damage->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = UASHealthAttributeSet::GetHealthAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FScalableFloat(-25.0f);
		Abilities->ApplyGameplayEffectToSelf(Damage, 1.0f, Abilities->MakeEffectContext());
		TestEqual(TEXT("Damage works before the result"), Health->GetHealth(), 75.0f);
		TestFalse(TEXT("Abilities are allowed before game end"), Abilities->AreAbilityTagsBlocked(FGameplayTagContainer()));

		State->SetPhase(Result, 0.0f);
		TestTrue(TEXT("The result is terminal"), State->IsGameEnded());
		TestTrue(TEXT("New abilities are blocked after game end"), Abilities->AreAbilityTagsBlocked(FGameplayTagContainer()));
		TestFalse(TEXT("The world remains unpaused for network and UI"), World->IsPaused());
		TestTrue(TEXT("Mass simulation is paused independently"), World->GetSubsystem<UMassSimulationSubsystem>()->IsSimulationPaused());
		TestTrue(TEXT("Existing character velocity is cleared"), Character->GetVelocity().IsNearlyZero());
		TestFalse(TEXT("Character movement stops ticking"), Character->GetCharacterMovement()->IsComponentTickEnabled());
		TestFalse(TEXT("Projectiles stop ticking"), Projectile->IsActorTickEnabled());
		TestFalse(TEXT("Projectiles cannot collide"), Projectile->GetActorEnableCollision());
		TestTrue(TEXT("The controller remains available for result UI"), Controller->IsActorTickEnabled());
		Abilities->ApplyGameplayEffectToSelf(Damage, 1.0f, Abilities->MakeEffectContext());
		TestEqual(TEXT("Late damage does not change health"), Health->GetHealth(), 75.0f);

		State->SetPhase(EGamePhase::Day, 120.0f);
		TestEqual(TEXT("Late phase transitions cannot overwrite the result"), State->GetCurrentPhase(), Result);
		bool bTimerFired = false;
		FTimerHandle Timer;
		World->GetTimerManager().SetTimer(Timer, [&bTimerFired]() { bTimerFired = true; }, 0.01f, false);
		World->Tick(LEVELTICK_All, 0.1f);
		// TimerManager only ticks once per engine frame, even when the test ticks its world twice.
		++GFrameCounter;
		World->Tick(LEVELTICK_All, 0.1f);
		TestTrue(TEXT("World timers continue after game end"), bTimerFired);
		World->GetTimerManager().ClearTimer(Timer);
		Instance->Shutdown();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}
	return true;
}

#endif
