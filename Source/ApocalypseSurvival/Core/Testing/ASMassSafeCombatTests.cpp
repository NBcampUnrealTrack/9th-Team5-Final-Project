#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/Combat/ASWeaponProjectile.h"
#include "GameplayEffect.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassSimulationSubsystem.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASMassSafeCombatTest,
	"ApocalypseSurvival.Combat.MassBusyHitsAreDeferred",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASMassSafeCombatTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(FName(TEXT("MassSafeCombatTest")));
	UWorld* World = Instance->GetWorld();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	World->GetSubsystem<UMassSimulationSubsystem>()->PauseSimulation();
	UASCombatTargetSubsystem* Combat = World->GetSubsystem<UASCombatTargetSubsystem>();
	UASMassMonsterSubsystem* Monsters = World->GetSubsystem<UASMassMonsterSubsystem>();
	FMassEntityManager& Manager = World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	UMassEntityConfigAsset* Config = LoadObject<UMassEntityConfigAsset>(nullptr,
		TEXT("/Game/Mass/Monster/DA_MassMonster_Melee.DA_MassMonster_Melee"));
	TArray<FMassEntityHandle> Entities;
	const FTransform SpawnTransform(FVector(100.0f, 0.0f, 200.0f));
	if (Config) Monsters->SpawnMonsters(*Config, MakeArrayView(&SpawnTransform, 1), EASMassMonsterWorldBehavior::NightAssault, Entities);
	if (!TestEqual(TEXT("One real Mass monster is spawned"), Entities.Num(), 1))
	{
		Instance->Shutdown();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}
	const FMassEntityHandle Target = Entities[0];
	UGameplayEffect* Effect = NewObject<UGameplayEffect>();
	FGameplayEffectSpecHandle Spec(new FGameplayEffectSpec(Effect, FGameplayEffectContextHandle(), 1.0f));
	Spec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -20.0f);
	const TArray<FGameplayEffectSpecHandle> Specs = { Spec };
	auto AdvanceTimers = [World]()
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, 0.05f);
		++GFrameCounter;
		World->Tick(LEVELTICK_All, 0.05f);
	};

	for (EASCombatTargetShape Shape : { EASCombatTargetShape::Radius, EASCombatTargetShape::Cone, EASCombatTargetShape::Segment })
	{
		Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth = 100.0f;
		FASCombatTargetQuery Query;
		Query.Shape = Shape;
		Query.Domain = EASCombatTargetDomain::MassOnly;
		Query.SearchOrigin = FVector(0.0f, 0.0f, 200.0f);
		Query.EndLocation = FVector(200.0f, 0.0f, 200.0f);
		Query.Direction = FVector::ForwardVector;
		Query.SearchRadius = Shape == EASCombatTargetShape::Segment ? 35.0f : 300.0f;
		Query.AngleDegrees = 60.0f;
		int32 Calls = 0;
		int32 Hits = 0;
		FTimerHandle Pending;
		{
			auto Scope = Manager.NewProcessingScope();
			TestEqual(TEXT("The unsafe direct mutation remains blocked"), Combat->ResolveAndApply(Query, Specs).NonActorHitCount, 0);
			FTimerDelegate Callback = FTimerDelegate::CreateLambda([&]()
			{
				TestFalse(TEXT("Deferred hit runs outside Mass processing"), Manager.IsProcessing());
				++Calls;
				Hits += Combat->ResolveAndApply(Query, Specs).NonActorHitCount;
			});
			TestTrue(TEXT("A busy Mass phase defers the hit"), Combat->DeferUntilMassIdle(Pending, Callback));
			Combat->DeferUntilMassIdle(Pending, Callback);
			TestEqual(TEXT("No callback executes inside the processing scope"), Calls, 0);
		}
		AdvanceTimers();
		TestEqual(TEXT("Repeated requests share one pending callback"), Calls, 1);
		TestEqual(TEXT("Deferred attack hits the monster once"), Hits, 1);
		TestEqual(TEXT("Deferred attack removes exactly 20 health"), Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth, 80.0f);
	}

	int32 CancelledCalls = 0;
	FTimerHandle CancelledTimer;
	{
		auto Scope = Manager.NewProcessingScope();
		Combat->DeferUntilMassIdle(CancelledTimer, FTimerDelegate::CreateLambda([&]() { ++CancelledCalls; }));
		World->GetTimerManager().ClearTimer(CancelledTimer);
	}
	AdvanceTimers();
	TestEqual(TEXT("Cancelled attacks cannot execute a stale callback"), CancelledCalls, 0);

	Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth = 100.0f;
	AASSkillProjectileActor* Projectile = World->SpawnActorDeferred<AASSkillProjectileActor>(
		AASSkillProjectileActor::StaticClass(), FTransform(FVector(0.0f, 0.0f, 200.0f)));
	Projectile->InitProjectile(Spec, nullptr, FVector::ForwardVector, 1000.0f, 5000.0f, 0, false);
	Projectile->FinishSpawning(FTransform(FVector(0.0f, 0.0f, 200.0f)));
	TestTrue(TEXT("Projectile ticks depend on all Mass phases"), Projectile->PrimaryActorTick.GetPrerequisites().Num() >= static_cast<int32>(EMassProcessingPhase::MAX));
	{
		auto Scope = Manager.NewProcessingScope();
		Projectile->SetActorLocation(FVector(200.0f, 0.0f, 200.0f));
		Projectile->Tick(0.05f);
		TestEqual(TEXT("The projectile does not mutate a busy entity"), Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth, 100.0f);
		++GFrameCounter;
		World->GetTimerManager().Tick(0.05f);
		++GFrameCounter;
		World->GetTimerManager().Tick(0.05f);
		TestEqual(TEXT("A retry stays pending if Mass is still busy"), Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth, 100.0f);
	}
	AdvanceTimers();
	TestEqual(TEXT("The projectile retains its swept segment until damage can be applied"), Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth, 80.0f);
	AdvanceTimers();
	TestEqual(TEXT("The same projectile does not apply damage twice"), Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth, 80.0f);

	Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth = 100.0f;
	Projectile = World->SpawnActorDeferred<AASSkillProjectileActor>(AASSkillProjectileActor::StaticClass(), FTransform(FVector(0.0f, 0.0f, 200.0f)));
	Projectile->InitProjectile(Spec, nullptr, FVector::ForwardVector, 1000.0f, 5000.0f, 0, false);
	Projectile->FinishSpawning(FTransform(FVector(0.0f, 0.0f, 200.0f)));
	{
		auto Scope = Manager.NewProcessingScope();
		Projectile->SetActorLocation(FVector(200.0f, 0.0f, 200.0f));
		FHitResult Stop;
		Stop.ImpactPoint = Projectile->GetActorLocation();
		Projectile->FindComponentByClass<UProjectileMovementComponent>()->StopSimulating(Stop);
		TestFalse(TEXT("A stopped projectile waits for its pending hit before destruction"), Projectile->IsActorBeingDestroyed());
	}
	AdvanceTimers();
	TestEqual(TEXT("Stopping against a wall does not discard the final hit segment"), Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth, 80.0f);

	Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth = 100.0f;
	FASCombatTargetQuery TargetQuery;
	TargetQuery.SearchRadius = 1000.0f;
	TargetQuery.Domain = EASCombatTargetDomain::MassOnly;
	FASCombatTargetRef TargetRef;
	TestTrue(TEXT("The homing projectile resolves its Mass target"), Combat->FindBestTarget(TargetQuery, TargetRef));
	AASWeaponProjectile* Homing = World->SpawnActorDeferred<AASWeaponProjectile>(AASWeaponProjectile::StaticClass(), FTransform(FVector(0.0f, 0.0f, 200.0f)));
	Homing->InitHoming(Specs, nullptr, TargetRef, 1000.0f);
	Homing->FinishSpawning(FTransform(FVector(0.0f, 0.0f, 200.0f)));
	TestTrue(TEXT("Homing refresh and impact wait for Mass phases"), Homing->PrimaryActorTick.GetPrerequisites().Num() >= static_cast<int32>(EMassProcessingPhase::MAX));
	AdvanceTimers();
	TestEqual(TEXT("The homing projectile applies damage to its target"), Manager.GetFragmentDataChecked<FASMassMonsterHealthFragment>(Target).CurrentHealth, 80.0f);
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
