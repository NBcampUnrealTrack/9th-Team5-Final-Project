#include "Mass/Monster/Testing/ASMassMonsterFunctionalTestActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ASAbilityTags.h"
#include "MassEntityConfigAsset.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterFunctionalTestActor)

AASMassMonsterFunctionalTestActor::AASMassMonsterFunctionalTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AASMassMonsterFunctionalTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(StartTimer, this,
		&AASMassMonsterFunctionalTestActor::TryStartTest, FMath::Max(0.01f, InitialDelay), false);
}

void AASMassMonsterFunctionalTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StartTimer);
	GetWorldTimerManager().ClearTimer(SpawnBatchTimer);
	GetWorldTimerManager().ClearTimer(StateReportTimer);
	RemoveGrantedInvulnerability();
	Super::EndPlay(EndPlayReason);
}

void AASMassMonsterFunctionalTestActor::TryStartTest()
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(EntityConfig))
	{
		UE_LOG(LogTemp, Error, TEXT("[MassFunctionalTest] EntityConfig is not assigned."));
		return;
	}

	if (UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
	{
		GetWorldTimerManager().SetTimer(StartTimer, this,
			&AASMassMonsterFunctionalTestActor::TryStartTest, 0.25f, false);
		return;
	}

	if (!BuildSpawnTransforms())
	{
		UE_LOG(LogTemp, Error, TEXT("[MassFunctionalTest] Could not create %d projected NavMesh spawn transforms."), MonsterCount);
		return;
	}

	if (bGrantInvulnerability)
	{
		GrantPlayerInvulnerability();
	}

	NextSpawnIndex = 0;
	SpawnNextBatch();
	if (NextSpawnIndex < PendingSpawnTransforms.Num())
	{
		GetWorldTimerManager().SetTimer(SpawnBatchTimer, this,
			&AASMassMonsterFunctionalTestActor::SpawnNextBatch, FMath::Max(0.01f, SpawnInterval), true);
	}
}

void AASMassMonsterFunctionalTestActor::SpawnNextBatch()
{
	UWorld* World = GetWorld();
	UASMassMonsterSubsystem* MonsterSubsystem = World ? World->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	if (!MonsterSubsystem || NextSpawnIndex >= PendingSpawnTransforms.Num())
	{
		GetWorldTimerManager().ClearTimer(SpawnBatchTimer);
		return;
	}

	const int32 BatchCount = FMath::Min(FMath::Max(1, SpawnBatchSize), PendingSpawnTransforms.Num() - NextSpawnIndex);
	TArray<FTransform> Batch;
	Batch.Append(PendingSpawnTransforms.GetData() + NextSpawnIndex, BatchCount);

	const FASMassMonsterSpawnResult Result = MonsterSubsystem->SpawnMonstersAtTransforms(EntityConfig, Batch, WorldBehavior);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogTemp, Error, TEXT("[MassFunctionalTest] Spawn failed at %d/%d: %s"),
			NextSpawnIndex, PendingSpawnTransforms.Num(), *Result.ErrorMessage);
		GetWorldTimerManager().ClearTimer(SpawnBatchTimer);
		return;
	}

	NextSpawnIndex += Result.SpawnedCount;
	if (NextSpawnIndex >= PendingSpawnTransforms.Num())
	{
		GetWorldTimerManager().ClearTimer(SpawnBatchTimer);
		UE_LOG(LogTemp, Display, TEXT("[MassFunctionalTest] Spawn complete: %d Mass monsters."), NextSpawnIndex);
		GetWorldTimerManager().SetTimer(StateReportTimer, this,
			&AASMassMonsterFunctionalTestActor::ReportStateCounts, FMath::Max(0.01f, StateReportDelay), false);
	}
}

void AASMassMonsterFunctionalTestActor::ReportStateCounts()
{
	UWorld* World = GetWorld();
	const UASMassMonsterSubsystem* MonsterSubsystem = World ? World->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	if (!MonsterSubsystem)
	{
		return;
	}

	const FASMassMonsterStateCounts Counts = MonsterSubsystem->GetManagedMonsterStateCounts();
	UE_LOG(LogTemp, Display,
		TEXT("[MassFunctionalTest] States Total=%d Idle=%d Chase=%d Attack=%d ReturnHome=%d Dead=%d"),
		Counts.Total, Counts.Idle, Counts.Chase, Counts.Attack, Counts.ReturnHome, Counts.Dead);
}

bool AASMassMonsterFunctionalTestActor::BuildSpawnTransforms()
{
	PendingSpawnTransforms.Reset();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!NavigationSystem || MonsterCount <= 0)
	{
		return false;
	}

	FVector Center = GetActorLocation();
	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		if (const APawn* PlayerPawn = PlayerController->GetPawn())
		{
			Center = PlayerPawn->GetActorLocation();
		}
	}

	const float SafeMinRadius = FMath::Max(0.0f, MinSpawnRadius);
	const float SafeMaxRadius = FMath::Max(SafeMinRadius + 1.0f, MaxSpawnRadius);
	constexpr double GoldenAngleRadians = 2.39996322972865332;
	const FVector ProjectionExtent(250.0f, 250.0f, 500.0f);

	for (int32 Index = 0; Index < MonsterCount; ++Index)
	{
		const double Fraction = (static_cast<double>(Index) + 0.5) / static_cast<double>(MonsterCount);
		const double RadiusSquared = FMath::Lerp(
			FMath::Square(static_cast<double>(SafeMinRadius)),
			FMath::Square(static_cast<double>(SafeMaxRadius)),
			Fraction);
		const double Radius = FMath::Sqrt(RadiusSquared);
		const double Angle = static_cast<double>(Index) * GoldenAngleRadians;
		const FVector DesiredLocation = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);

		FNavLocation ProjectedLocation;
		if (!NavigationSystem->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, ProjectionExtent))
		{
			continue;
		}

		const FRotator FacingRotation = (Center - ProjectedLocation.Location).Rotation();
		PendingSpawnTransforms.Emplace(FacingRotation, ProjectedLocation.Location);
	}

	return PendingSpawnTransforms.Num() == MonsterCount;
}

void AASMassMonsterFunctionalTestActor::GrantPlayerInvulnerability()
{
	ModifiedAbilitySystems.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		UAbilitySystemComponent* ASC = IsValid(PlayerPawn)
			? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerPawn)
			: nullptr;
		if (!IsValid(ASC))
		{
			continue;
		}

		ASC->AddLooseGameplayTag(ASGameplayTags::State_Invulnerable, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
		ModifiedAbilitySystems.Add(ASC);
	}
}

void AASMassMonsterFunctionalTestActor::RemoveGrantedInvulnerability()
{
	for (const TWeakObjectPtr<UAbilitySystemComponent>& WeakASC : ModifiedAbilitySystems)
	{
		if (UAbilitySystemComponent* ASC = WeakASC.Get())
		{
			ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Invulnerable, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
	ModifiedAbilitySystems.Reset();
}
