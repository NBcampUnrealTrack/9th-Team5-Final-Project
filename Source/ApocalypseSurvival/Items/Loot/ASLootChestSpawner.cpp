#include "Items/Loot/ASLootChestSpawner.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Items/Loot/ASLootChest.h"
#include "Items/Loot/ASLootSpawnSubsystem.h"
#include "Templates/UnrealTemplate.h"

AASLootChestSpawner::AASLootChestSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AASLootChestSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (bSpawnOnBeginPlay && HasAuthority() && GetNetMode() != NM_Client)
	{
		const FASLootChestSpawnResult Result = SpawnChest();
		if (!Result.IsSuccess())
			UE_LOG(LogTemp, Warning, TEXT("[LootChestSpawner] %s: %s"), *GetName(), *Result.ErrorMessage);
	}
}

AASLootChest* AASLootChestSpawner::GetSpawnedChest() const
{
	AASLootChest* Chest = SpawnedChest.Get();
	return IsValid(Chest) && !Chest->IsActorBeingDestroyed() ? Chest : nullptr;
}

FASLootChestSpawnResult AASLootChestSpawner::SpawnChest()
{
	FASLootChestSpawnResult Result;
	if (!HasAuthority() || GetNetMode() == NM_Client)
	{
		Result.Status = EASLootChestSpawnStatus::NotAuthority;
		Result.ErrorMessage = TEXT("Loot chest spawners can only spawn on the server.");
		return Result;
	}
	if (IsActorBeingDestroyed() || bSpawnInProgress)
	{
		Result.ErrorMessage = TEXT("The spawner is being destroyed or is already spawning a chest.");
		return Result;
	}
	if (AASLootChest* ExistingChest = GetSpawnedChest())
	{
		Result.Chest = ExistingChest;
		Result.Status = EASLootChestSpawnStatus::Success;
		return Result;
	}

	// Construction/BeginPlay callbacks must not recursively spawn another chest.
	TGuardValue<bool> SpawnGuard(bSpawnInProgress, true);
	if (!bRandomInitialized)
	{
		Random.Initialize(bUseRandomSeed ? FMath::Rand() : Seed);
		bRandomInitialized = true;
	}
	FRandomStream AttemptRandom = Random;
	const int32 ChestSeed = static_cast<int32>(AttemptRandom.GetUnsignedInt());
	UASLootSpawnSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UASLootSpawnSubsystem>() : nullptr;
	if (!Subsystem)
	{
		Result.Status = EASLootChestSpawnStatus::InvalidWorld;
		Result.ErrorMessage = TEXT("The spawner requires a server game world with a loot spawn subsystem.");
		return Result;
	}
	Result = Subsystem->SpawnChestAtTransform(LootPreset, GetActorTransform(), ChestSeed);
	if (Result.IsSuccess())
	{
		// A chest callback can destroy this spawner while the API finishes spawning.
		if (IsActorBeingDestroyed())
		{
			Result.Chest->Destroy();
			Result.Chest = nullptr;
			Result.Status = EASLootChestSpawnStatus::SpawnFailed;
			Result.ErrorMessage = TEXT("The spawner was destroyed while creating its chest.");
			return Result;
		}
		SpawnedChest = Result.Chest.Get();
		Random = AttemptRandom; // Failed attempts leave the next loot roll unchanged.
	}
	return Result;
}
