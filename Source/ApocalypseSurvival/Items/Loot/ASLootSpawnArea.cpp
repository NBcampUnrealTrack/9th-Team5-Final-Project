#include "Items/Loot/ASLootSpawnArea.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"

AASLootSpawnArea::AASLootSpawnArea()
{
	PrimaryActorTick.bCanEverTick = false;
	SpawnBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBounds"));
	SetRootComponent(SpawnBounds);
	SpawnBounds->InitBoxExtent(FVector(3000.0, 3000.0, 1000.0));
	SpawnBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnBounds->SetGenerateOverlapEvents(false);
	SpawnBounds->SetHiddenInGame(true);
}

void AASLootSpawnArea::BeginPlay()
{
	Super::BeginPlay();
	if (bSpawnOnBeginPlay && HasAuthority() && GetNetMode() != NM_Client)
	{
		const FASLootSpawnBatchResult Result = SpawnChests();
		if (Result.Status != EASLootSpawnBatchStatus::Success)
			UE_LOG(LogTemp, Warning, TEXT("[LootSpawnArea] %s: %s"), *GetName(), *Result.ErrorMessage);
	}
}

FASLootSpawnBatchResult AASLootSpawnArea::SpawnChests()
{
	UASLootSpawnSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UASLootSpawnSubsystem>() : nullptr;
	if (!HasAuthority() || GetNetMode() == NM_Client || !Subsystem)
	{
		FASLootSpawnBatchResult Result;
		Result.RequestedCount = SpawnSettings.Count;
		Result.Status = EASLootSpawnBatchStatus::NotAuthority;
		Result.ErrorMessage = TEXT("A loot spawn area requires a server game world with a loot spawn subsystem.");
		return Result;
	}
	return Subsystem->SpawnChestsInBox(LootPreset, SpawnBounds->GetComponentTransform(),
		SpawnBounds->GetUnscaledBoxExtent(), SpawnSettings, bUseRandomSeed ? FMath::Rand() : Seed);
}
