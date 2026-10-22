#include "Items/Loot/ASMonsterDropSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/Loot/ASDroppedItem.h"
#include "Items/Loot/ASMonsterDropDefinition.h"
#include "Items/Loot/ASMonsterDropGenerator.h"
#include "CollisionQueryParams.h"

bool UASMonsterDropSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

bool UASMonsterDropSubsystem::SpawnDrops(const FDataTableRowHandle& DropGroup, const FVector& DeathLocation,
	AController* KillerController)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !IsValid(KillerController)
		|| !KillerController->IsPlayerController() || !IsValid(DropGroup.DataTable) || DropGroup.RowName.IsNone())
	{
		return false;
	}
	if (DropGroup.DataTable->GetRowStruct() != FASMonsterDropGroupRow::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MonsterDrop] %s does not use FASMonsterDropGroupRow."), *DropGroup.RowName.ToString());
		return false;
	}

	const FASMonsterDropGroupRow* Group = DropGroup.DataTable->FindRow<FASMonsterDropGroupRow>(
		DropGroup.RowName, TEXT("MonsterDrop"), false);
	if (!Group)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MonsterDrop] Drop group '%s' does not exist."), *DropGroup.RowName.ToString());
		return false;
	}

	FRandomStream Random(FMath::Rand());
	// 0%는 절대 드랍하지 않고, 100%는 항상 드랍한다.
	if (Group->DropChance <= 0.0f || Random.FRand() >= Group->DropChance)
	{
		return true;
	}

	TArray<FASItemGrant> Grants;
	FString Error;
	if (!FASMonsterDropGenerator::Generate(*Group, Random, Grants, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MonsterDrop] Failed to generate '%s': %s"),
			*DropGroup.RowName.ToString(), *Error);
		return false;
	}

	bool bSpawnedAny = false;
	int32 DropIndex = 0;
	for (const FASItemGrant& Grant : Grants)
	{
		if (!IsValid(Grant.ItemDefinition) || Grant.Quantity <= 0)
		{
			continue;
		}

		int32 RemainingQuantity = Grant.Quantity;
		const int32 MaxStackSize = FMath::Max(1, Grant.ItemDefinition->MaxStackSize);
		while (RemainingQuantity > 0)
		{
			const int32 StackQuantity = FMath::Min(RemainingQuantity, MaxStackSize);
			const FVector Location = FindDropLocation(DeathLocation, DropIndex++, Random);
			bSpawnedAny |= SpawnDropStack(Grant, StackQuantity, Location) != nullptr;
			RemainingQuantity -= StackQuantity;
		}
	}
	return bSpawnedAny;
}

AASDroppedItem* UASMonsterDropSubsystem::SpawnDropStack(const FASItemGrant& Grant, const int32 Quantity,
	const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(Grant.ItemDefinition) || Quantity <= 0)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AASDroppedItem* DroppedItem = World->SpawnActor<AASDroppedItem>(Location, FRotator::ZeroRotator, SpawnParameters);
	if (!DroppedItem)
	{
		return nullptr;
	}

	UASInventoryComponent* Inventory = DroppedItem->GetLootInventory();
	if (!Inventory || Inventory->AddItem(Grant.ItemDefinition, Quantity) != Quantity)
	{
		DroppedItem->Destroy();
		return nullptr;
	}
	return DroppedItem;
}

FVector UASMonsterDropSubsystem::FindDropLocation(const FVector& Origin, const int32 DropIndex, FRandomStream& Random) const
{
	UWorld* World = GetWorld();
	const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
	const float Radius = 45.0f + 30.0f * FMath::Sqrt(static_cast<float>(DropIndex + 1));
	FVector Location = Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;
	if (!World)
	{
		return Location;
	}

	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterDropGround));
	if (World->LineTraceSingleByChannel(GroundHit, Location + FVector(0.0f, 0.0f, 250.0f),
		Location - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility, QueryParams))
	{
		Location = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, 30.0f);
	}
	return Location;
}
