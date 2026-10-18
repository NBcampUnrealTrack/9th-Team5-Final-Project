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
	DestroyAllManagedMonsters();
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
