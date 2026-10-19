#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassEntityTemplate.h"
#include "MassSpawnLocationProcessor.h"
#include "MassSpawnerSubsystem.h"
#include "MassSpawnerTypes.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Subsystems/ASMassPlayerTargetSubsystem.h"
#include "StructUtils/StructView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterSubsystem)

namespace
{
	constexpr int32 MaxMassMonsterSpawnBatchSize = 10000;
}

bool UASMassMonsterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

void UASMassMonsterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassSpawnerSubsystem>();
	SpawnerSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UMassSpawnerSubsystem>() : nullptr;
}

void UASMassMonsterSubsystem::Deinitialize()
{
	// UMassSpawnerSubsystem의 Entity Manager가 먼저 Deinitialize될 수 있다.
	// 월드 종료 시에는 Mass가 전체 Entity 저장소를 정리하므로 Handle만 폐기한다.
	ManagedEntities.Reset();
	SpawnerSubsystem = nullptr;
	Super::Deinitialize();
}

FASMassMonsterSpawnResult UASMassMonsterSubsystem::SpawnMonstersAtTransforms(
	UMassEntityConfigAsset* EntityConfig,
	const TArray<FTransform>& SpawnTransforms,
	const EASMassMonsterWorldBehavior WorldBehavior)
{
	FASMassMonsterSpawnResult Result;
	if (!IsValid(EntityConfig))
	{
		Result.Status = EASMassMonsterSpawnStatus::InvalidEntityConfig;
		Result.RequestedCount = SpawnTransforms.Num();
		Result.ErrorMessage = TEXT("A valid Mass Entity Config is required.");
		return Result;
	}

	TArray<FMassEntityHandle> SpawnedEntities;
	return SpawnMonsters(*EntityConfig, SpawnTransforms, WorldBehavior, SpawnedEntities);
}

FASMassMonsterSpawnResult UASMassMonsterSubsystem::SpawnMonsters(
	UMassEntityConfigAsset& EntityConfig,
	const TConstArrayView<FTransform> SpawnTransforms,
	const EASMassMonsterWorldBehavior WorldBehavior,
	TArray<FMassEntityHandle>& OutEntities)
{
	FASMassMonsterSpawnResult Result;
	Result.RequestedCount = SpawnTransforms.Num();
	OutEntities.Reset();

	FString MutationError;
	if (!CanMutateEntities(MutationError))
	{
		const UWorld* World = GetWorld();
		Result.Status = World && World->GetNetMode() == NM_Client
			? EASMassMonsterSpawnStatus::NotAuthority
			: EASMassMonsterSpawnStatus::MassUnavailable;
		Result.ErrorMessage = MoveTemp(MutationError);
		return Result;
	}

	if (bMutationInProgress || SpawnTransforms.IsEmpty()
		|| SpawnTransforms.Num() > MaxMassMonsterSpawnBatchSize)
	{
		Result.Status = EASMassMonsterSpawnStatus::InvalidRequest;
		Result.ErrorMessage = FString::Printf(
			TEXT("Provide 1..%d transforms and do not start a nested Mass monster mutation."),
			MaxMassMonsterSpawnBatchSize);
		return Result;
	}

	for (const FTransform& Transform : SpawnTransforms)
	{
		if (Transform.ContainsNaN())
		{
			Result.Status = EASMassMonsterSpawnStatus::InvalidRequest;
			Result.ErrorMessage = TEXT("A spawn transform contains non-finite values.");
			return Result;
		}
	}

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	CompactManagedEntities();

	const FMassEntityTemplate& EntityTemplate = EntityConfig.GetOrCreateEntityTemplate(*GetWorld());
	const FMassElementBitSet& Elements = EntityTemplate.GetCompositionDescriptor().GetElementsBitSet();
	if (!EntityTemplate.IsValid()
		|| !Elements.Contains<FTransformFragment>()
		|| !Elements.Contains<FASMassMonsterHomeFragment>()
		|| !Elements.Contains<FASMassMonsterBehaviorFragment>())
	{
		Result.Status = EASMassMonsterSpawnStatus::InvalidEntityConfig;
		Result.ErrorMessage = TEXT("The Mass Entity Config must contain AS Mass Monster Trait and its required fragments.");
		return Result;
	}

	FMassTransformsSpawnData SpawnData;
	SpawnData.Transforms.Append(SpawnTransforms);
	SpawnData.bRandomize = false;

	TArray<FMassEntityHandle> SpawnedEntities;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext = SpawnerSubsystem->SpawnEntities(
		EntityTemplate.GetTemplateID(),
		static_cast<uint32>(SpawnTransforms.Num()),
		FConstStructView::Make(SpawnData),
		UMassSpawnLocationProcessor::StaticClass(),
		SpawnedEntities);

	if (SpawnedEntities.Num() != SpawnTransforms.Num())
	{
		if (!SpawnedEntities.IsEmpty())
		{
			CreationContext.Reset();
			SpawnerSubsystem->DestroyEntities(SpawnedEntities);
		}
		Result.Status = EASMassMonsterSpawnStatus::SpawnFailed;
		Result.SpawnedCount = 0;
		Result.ErrorMessage = FString::Printf(TEXT("Mass spawned %d/%d requested monsters."),
			SpawnedEntities.Num(), SpawnTransforms.Num());
		return Result;
	}

	FMassEntityManager& EntityManager = SpawnerSubsystem->GetEntityManagerChecked();
	for (int32 Index = 0; Index < SpawnedEntities.Num(); ++Index)
	{
		const FMassEntityHandle Entity = SpawnedEntities[Index];
		FASMassMonsterHomeFragment& Home = EntityManager.GetFragmentDataChecked<FASMassMonsterHomeFragment>(Entity);
		Home.HomeLocation = SpawnTransforms[Index].GetLocation();

		FASMassMonsterBehaviorFragment& Behavior =
			EntityManager.GetFragmentDataChecked<FASMassMonsterBehaviorFragment>(Entity);
		Behavior.State = EASMassMonsterState::Idle;
		Behavior.WorldBehavior = WorldBehavior;
	}

	ManagedEntities.Append(SpawnedEntities);
	OutEntities = SpawnedEntities;
	Result.Status = EASMassMonsterSpawnStatus::Success;
	Result.SpawnedCount = SpawnedEntities.Num();
	CreationContext.Reset();
	return Result;
}

int32 UASMassMonsterSubsystem::ConfigureMonsterHomes(
	const TConstArrayView<FMassEntityHandle> Entities,
	const FASMassMonsterHomeSettings& HomeSettings)
{
	FString MutationError;
	if (!CanMutateEntities(MutationError) || bMutationInProgress || Entities.IsEmpty())
	{
		return 0;
	}

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	CompactManagedEntities();
	TSet<FMassEntityHandle> ManagedEntitySet;
	ManagedEntitySet.Reserve(ManagedEntities.Num());
	for (const FMassEntityHandle ManagedEntity : ManagedEntities)
	{
		ManagedEntitySet.Add(ManagedEntity);
	}
	FMassEntityManager& EntityManager = SpawnerSubsystem->GetEntityManagerChecked();
	int32 ConfiguredCount = 0;
	for (const FMassEntityHandle Entity : Entities)
	{
		if (!ManagedEntitySet.Contains(Entity) || !EntityManager.IsEntityValid(Entity))
		{
			continue;
		}

		FASMassMonsterHomeFragment* Home = EntityManager.GetFragmentDataPtr<FASMassMonsterHomeFragment>(Entity);
		if (!Home)
		{
			continue;
		}

		Home->LeashRadius = FMath::Max(0.0f, HomeSettings.LeashRadius);
		Home->ReturnAcceptanceRadius = FMath::Max(1.0f, HomeSettings.ReturnAcceptanceRadius);
		Home->DetectionRadius = FMath::Max(0.0f, HomeSettings.DetectionRadius);
		Home->LoseTargetRadius = FMath::Max(Home->DetectionRadius, HomeSettings.LoseTargetRadius);
		Home->ReturnHealthPercentPerSecond = FMath::Max(0.0f, HomeSettings.ReturnHealthPercentPerSecond);
		Home->ReturnMoveSpeedMultiplier = FMath::Max(1.0f, HomeSettings.ReturnMoveSpeedMultiplier);
		++ConfiguredCount;
	}
	return ConfiguredCount;
}

int32 UASMassMonsterSubsystem::SetManagedMonsterWorldBehavior(
	const EASMassMonsterWorldBehavior WorldBehavior)
{
	FString MutationError;
	if (!CanMutateEntities(MutationError) || bMutationInProgress)
	{
		return 0;
	}

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	CompactManagedEntities();
	FMassEntityManager& EntityManager = SpawnerSubsystem->GetEntityManagerChecked();
	int32 UpdatedCount = 0;
	for (const FMassEntityHandle Entity : ManagedEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}

		FASMassMonsterHealthFragment* Health = EntityManager.GetFragmentDataPtr<FASMassMonsterHealthFragment>(Entity);
		FASMassMonsterBehaviorFragment* Behavior = EntityManager.GetFragmentDataPtr<FASMassMonsterBehaviorFragment>(Entity);
		FASMassMonsterTargetFragment* Target = EntityManager.GetFragmentDataPtr<FASMassMonsterTargetFragment>(Entity);
		if (!Health || Health->CurrentHealth <= 0.0f || !Behavior || !Target)
		{
			continue;
		}

		Behavior->WorldBehavior = WorldBehavior;
		Behavior->State = EASMassMonsterState::Idle;
		Target->Reset();
		++UpdatedCount;
	}
	return UpdatedCount;
}

bool UASMassMonsterSubsystem::DestroyMonster(const FMassEntityHandle Entity)
{
	return DestroyMonsters(MakeArrayView(&Entity, 1)) == 1;
}

int32 UASMassMonsterSubsystem::DestroyMonsters(const TConstArrayView<FMassEntityHandle> Entities)
{
	FString MutationError;
	if (!CanMutateEntities(MutationError) || bMutationInProgress || Entities.IsEmpty())
	{
		return 0;
	}

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	CompactManagedEntities();

	TSet<FMassEntityHandle> RequestedEntities;
	RequestedEntities.Reserve(Entities.Num());
	for (const FMassEntityHandle Entity : Entities)
	{
		RequestedEntities.Add(Entity);
	}
	TArray<FMassEntityHandle> EntitiesToDestroy;
	for (int32 Index = ManagedEntities.Num() - 1; Index >= 0; --Index)
	{
		if (RequestedEntities.Contains(ManagedEntities[Index]))
		{
			EntitiesToDestroy.Add(ManagedEntities[Index]);
			ManagedEntities.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}

	if (!EntitiesToDestroy.IsEmpty())
	{
		SpawnerSubsystem->DestroyEntities(EntitiesToDestroy);
	}
	return EntitiesToDestroy.Num();
}

void UASMassMonsterSubsystem::DestroyAllManagedMonsters()
{
	FString MutationError;
	if (!CanMutateEntities(MutationError) || bMutationInProgress)
	{
		return;
	}

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	CompactManagedEntities();
	if (!ManagedEntities.IsEmpty())
	{
		SpawnerSubsystem->DestroyEntities(ManagedEntities);
		ManagedEntities.Reset();
	}
}

int32 UASMassMonsterSubsystem::GetManagedMonsterCount() const
{
	if (!SpawnerSubsystem)
	{
		return 0;
	}

	const FMassEntityManager& EntityManager = SpawnerSubsystem->GetEntityManagerChecked();
	int32 Count = 0;
	for (const FMassEntityHandle Entity : ManagedEntities)
	{
		Count += EntityManager.IsEntityValid(Entity) ? 1 : 0;
	}
	return Count;
}

int32 UASMassMonsterSubsystem::GetAliveManagedMonsterCount() const
{
	if (!SpawnerSubsystem)
	{
		return 0;
	}

	const FMassEntityManager& EntityManager = SpawnerSubsystem->GetEntityManagerChecked();
	int32 Count = 0;
	for (const FMassEntityHandle Entity : ManagedEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}

		const FASMassMonsterHealthFragment* Health =
			EntityManager.GetFragmentDataPtr<FASMassMonsterHealthFragment>(Entity);
		Count += Health && Health->CurrentHealth > 0.0f ? 1 : 0;
	}
	return Count;
}

int32 UASMassMonsterSubsystem::ApplyDamageInRadius(
	const FVector& Center,
	const float Radius,
	const float Damage,
	AActor* DamageInstigator,
	const int32 MaxTargets)
{
	const float SafeRadius = FMath::Max(0.0f, Radius);
	return ApplyDamageToMatchingMonsters(Damage, DamageInstigator, MaxTargets,
		[Center, SafeRadius](const FVector& Location, const FASMassMonsterConfigFragment& Config)
		{
			return FVector::DistSquared2D(Center, Location) <= FMath::Square(SafeRadius + Config.HitRadius);
		});
}

int32 UASMassMonsterSubsystem::ApplyDamageInCone(
	const FVector& Center,
	const FVector& AimDirection,
	const float Radius,
	const float AngleDegrees,
	const float Damage,
	AActor* DamageInstigator,
	const int32 MaxTargets)
{
	const float SafeRadius = FMath::Max(0.0f, Radius);
	const float SafeAngle = FMath::Clamp(AngleDegrees, 1.0f, 360.0f);
	const FVector SafeAim = AimDirection.GetSafeNormal2D();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SafeAngle * 0.5f));
	return ApplyDamageToMatchingMonsters(Damage, DamageInstigator, MaxTargets,
		[Center, SafeRadius, SafeAngle, SafeAim, CosHalfAngle](
			const FVector& Location, const FASMassMonsterConfigFragment& Config)
		{
			const FVector Offset = Location - Center;
			if (Offset.SizeSquared2D() > FMath::Square(SafeRadius + Config.HitRadius))
			{
				return false;
			}
			if (SafeAngle >= 360.0f || SafeAim.IsNearlyZero())
			{
				return true;
			}
			const FVector Direction = Offset.GetSafeNormal2D();
			return Direction.IsNearlyZero() || FVector::DotProduct(SafeAim, Direction) >= CosHalfAngle;
		});
}

int32 UASMassMonsterSubsystem::ApplyDamageAlongSegment(
	const FVector& Start,
	const FVector& End,
	const float SweepRadius,
	const float Damage,
	AActor* DamageInstigator,
	const int32 MaxTargets)
{
	const FVector Start2D(Start.X, Start.Y, 0.0f);
	const FVector End2D(End.X, End.Y, 0.0f);
	const float SafeSweepRadius = FMath::Max(0.0f, SweepRadius);
	return ApplyDamageToMatchingMonsters(Damage, DamageInstigator, MaxTargets,
		[Start2D, End2D, SafeSweepRadius](const FVector& Location, const FASMassMonsterConfigFragment& Config)
		{
			const FVector Location2D(Location.X, Location.Y, 0.0f);
			return FMath::PointDistToSegmentSquared(Location2D, Start2D, End2D)
				<= FMath::Square(SafeSweepRadius + Config.HitRadius);
		});
}

int32 UASMassMonsterSubsystem::ApplyDamageAlongSegmentUnique(
	const FVector& Start,
	const FVector& End,
	const float SweepRadius,
	const float Damage,
	AActor* DamageInstigator,
	TSet<int32>& InOutHitEntityIndices,
	const int32 MaxTargets)
{
	FString MutationError;
	if (!CanMutateEntities(MutationError) || bMutationInProgress || Damage <= 0.0f)
	{
		return 0;
	}

	struct FCandidate
	{
		FMassEntityHandle Entity;
		float SegmentAlpha = 0.0f;
	};

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	CompactManagedEntities();
	FMassEntityManager& EntityManager = SpawnerSubsystem->GetEntityManagerChecked();
	const FVector Start2D(Start.X, Start.Y, 0.0f);
	const FVector End2D(End.X, End.Y, 0.0f);
	const FVector Segment = End2D - Start2D;
	const float SegmentSizeSquared = Segment.SizeSquared();
	const float SafeSweepRadius = FMath::Max(0.0f, SweepRadius);
	TArray<FCandidate> Candidates;

	for (const FMassEntityHandle Entity : ManagedEntities)
	{
		if (!EntityManager.IsEntityValid(Entity) || InOutHitEntityIndices.Contains(Entity.Index))
		{
			continue;
		}

		const FASMassMonsterHealthFragment* Health = EntityManager.GetFragmentDataPtr<FASMassMonsterHealthFragment>(Entity);
		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
		const FASMassMonsterConfigFragment* Config =
			EntityManager.GetConstSharedFragmentDataPtr<FASMassMonsterConfigFragment>(Entity);
		if (!Health || Health->CurrentHealth <= 0.0f || !Transform || !Config)
		{
			continue;
		}

		const FVector Location2D = Transform->GetTransform().GetLocation() * FVector(1.0f, 1.0f, 0.0f);
		if (FMath::PointDistToSegmentSquared(Location2D, Start2D, End2D)
			> FMath::Square(SafeSweepRadius + Config->HitRadius))
		{
			continue;
		}

		FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Entity = Entity;
		Candidate.SegmentAlpha = SegmentSizeSquared > SMALL_NUMBER
			? FMath::Clamp(FVector::DotProduct(Location2D - Start2D, Segment) / SegmentSizeSquared, 0.0f, 1.0f)
			: 0.0f;
	}

	Candidates.Sort([](const FCandidate& Left, const FCandidate& Right)
	{
		return Left.SegmentAlpha < Right.SegmentAlpha;
	});

	const UASMassPlayerTargetSubsystem* TargetSubsystem = GetWorld()->GetSubsystem<UASMassPlayerTargetSubsystem>();
	const FASMassPlayerTargetSnapshot* InstigatorSnapshot = TargetSubsystem
		? TargetSubsystem->FindTargetForActor(DamageInstigator)
		: nullptr;
	int32 DamagedCount = 0;
	for (const FCandidate& Candidate : Candidates)
	{
		if (MaxTargets > 0 && DamagedCount >= MaxTargets)
		{
			break;
		}

		FASMassMonsterHealthFragment* Health =
			EntityManager.GetFragmentDataPtr<FASMassMonsterHealthFragment>(Candidate.Entity);
		const FASMassMonsterConfigFragment* Config =
			EntityManager.GetConstSharedFragmentDataPtr<FASMassMonsterConfigFragment>(Candidate.Entity);
		if (!Health || Health->CurrentHealth <= 0.0f || !Config)
		{
			continue;
		}

		Health->CurrentHealth = FMath::Max(0.0f, Health->CurrentHealth - Damage);
		if (InstigatorSnapshot && InstigatorSnapshot->bAlive)
		{
			if (FASMassMonsterTargetFragment* Target =
				EntityManager.GetFragmentDataPtr<FASMassMonsterTargetFragment>(Candidate.Entity))
			{
				Target->TargetId = InstigatorSnapshot->TargetId;
				Target->CachedTargetLocation = InstigatorSnapshot->Location;
				Target->Aggro = FMath::Max(Target->Aggro, Config->DirectHitAggro);
				Target->RefreshTimeRemaining = 0.0f;
			}
		}

		InOutHitEntityIndices.Add(Candidate.Entity.Index);
		++DamagedCount;
	}
	return DamagedCount;
}

int32 UASMassMonsterSubsystem::ApplyDamageToMatchingMonsters(
	const float Damage,
	AActor* DamageInstigator,
	const int32 MaxTargets,
	TFunctionRef<bool(const FVector&, const FASMassMonsterConfigFragment&)> Predicate)
{
	FString MutationError;
	if (!CanMutateEntities(MutationError) || bMutationInProgress || Damage <= 0.0f)
	{
		return 0;
	}

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	CompactManagedEntities();
	FMassEntityManager& EntityManager = SpawnerSubsystem->GetEntityManagerChecked();
	const UASMassPlayerTargetSubsystem* TargetSubsystem = GetWorld()->GetSubsystem<UASMassPlayerTargetSubsystem>();
	const FASMassPlayerTargetSnapshot* InstigatorSnapshot = TargetSubsystem
		? TargetSubsystem->FindTargetForActor(DamageInstigator)
		: nullptr;

	int32 DamagedCount = 0;
	for (const FMassEntityHandle Entity : ManagedEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}

		FASMassMonsterHealthFragment* Health = EntityManager.GetFragmentDataPtr<FASMassMonsterHealthFragment>(Entity);
		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
		const FASMassMonsterConfigFragment* Config =
			EntityManager.GetConstSharedFragmentDataPtr<FASMassMonsterConfigFragment>(Entity);
		if (!Health || Health->CurrentHealth <= 0.0f || !Transform || !Config
			|| !Predicate(Transform->GetTransform().GetLocation(), *Config))
		{
			continue;
		}

		Health->CurrentHealth = FMath::Max(0.0f, Health->CurrentHealth - Damage);
		if (InstigatorSnapshot && InstigatorSnapshot->bAlive)
		{
			if (FASMassMonsterTargetFragment* Target =
				EntityManager.GetFragmentDataPtr<FASMassMonsterTargetFragment>(Entity))
			{
				Target->TargetId = InstigatorSnapshot->TargetId;
				Target->CachedTargetLocation = InstigatorSnapshot->Location;
				Target->Aggro = FMath::Max(Target->Aggro, Config->DirectHitAggro);
				Target->RefreshTimeRemaining = 0.0f;
			}
		}

		++DamagedCount;
		if (MaxTargets > 0 && DamagedCount >= MaxTargets)
		{
			break;
		}
	}
	return DamagedCount;
}

FASMassMonsterStateCounts UASMassMonsterSubsystem::GetManagedMonsterStateCounts() const
{
	FASMassMonsterStateCounts Counts;
	if (!SpawnerSubsystem)
	{
		return Counts;
	}

	const FMassEntityManager& EntityManager = SpawnerSubsystem->GetEntityManagerChecked();
	for (const FMassEntityHandle Entity : ManagedEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}

		const FASMassMonsterBehaviorFragment* Behavior =
			EntityManager.GetFragmentDataPtr<FASMassMonsterBehaviorFragment>(Entity);
		if (!Behavior)
		{
			continue;
		}

		++Counts.Total;
		switch (Behavior->State)
		{
		case EASMassMonsterState::Idle: ++Counts.Idle; break;
		case EASMassMonsterState::Chase: ++Counts.Chase; break;
		case EASMassMonsterState::Attack: ++Counts.Attack; break;
		case EASMassMonsterState::ReturnHome: ++Counts.ReturnHome; break;
		case EASMassMonsterState::Dead: ++Counts.Dead; break;
		default: break;
		}
	}

	return Counts;
}

bool UASMassMonsterSubsystem::CanMutateEntities(FString& OutError) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		OutError = TEXT("No world is available.");
		return false;
	}
	if (World->GetNetMode() == NM_Client)
	{
		OutError = TEXT("Mass monster mutations are server-only.");
		return false;
	}
	if (!SpawnerSubsystem)
	{
		OutError = TEXT("UMassSpawnerSubsystem is unavailable.");
		return false;
	}
	if (SpawnerSubsystem->GetEntityManagerChecked().IsProcessing())
	{
		OutError = TEXT("Mass Entity processing is active; defer this mutation until processing completes.");
		return false;
	}
	return true;
}

void UASMassMonsterSubsystem::CompactManagedEntities()
{
	if (!SpawnerSubsystem)
	{
		ManagedEntities.Reset();
		return;
	}

	const FMassEntityManager& EntityManager = SpawnerSubsystem->GetEntityManagerChecked();
	ManagedEntities.RemoveAllSwap([&EntityManager](const FMassEntityHandle Entity)
	{
		return !EntityManager.IsEntityValid(Entity);
	}, EAllowShrinking::No);
}
