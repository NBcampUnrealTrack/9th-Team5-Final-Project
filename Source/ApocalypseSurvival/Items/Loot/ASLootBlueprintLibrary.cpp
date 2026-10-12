#include "Items/Loot/ASLootBlueprintLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Loot/ASLootChest.h"
#include "Items/Loot/ASLootDefinition.h"
#include "Items/Loot/ASLootGenerator.h"

AASLootChest* UASLootBlueprintLibrary::SpawnLootChest(UObject* WorldContextObject,
	TSubclassOf<AASLootChest> ChestClass, const FTransform& SpawnTransform, const TArray<FASItemGrant>& Items)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_Client || !ChestClass
		|| ChestClass->HasAnyClassFlags(CLASS_Abstract) || SpawnTransform.ContainsNaN()) return nullptr;

	// Validate the whole list before spawning, rather than silently omitting bad entries.
	for (const FASItemGrant& Grant : Items)
	{
		if (!IsValid(Grant.ItemDefinition) || Grant.Quantity <= 0 || Grant.ItemDefinition->MaxStackSize <= 0)
			return nullptr;
	}

	AASLootChest* Chest = World->SpawnActorDeferred<AASLootChest>(ChestClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Chest) return nullptr;
	if (!Chest->SetInitialLoot(Items))
	{
		Chest->Destroy();
		return nullptr;
	}
	Chest->FinishSpawning(SpawnTransform);
	return IsValid(Chest) && !Chest->IsActorBeingDestroyed() ? Chest : nullptr;
}

namespace
{
	const FASLootChestPresetRow* ResolvePreset(const FDataTableRowHandle& Preset, FString& OutError)
	{
		OutError.Reset();
		if (!IsValid(Preset.DataTable) || Preset.DataTable->GetRowStruct() != FASLootChestPresetRow::StaticStruct())
		{
			OutError = TEXT("Preset DataTable must use FASLootChestPresetRow.");
			return nullptr;
		}
		if (Preset.RowName.IsNone()) { OutError = TEXT("Preset RowName is empty."); return nullptr; }
		const FASLootChestPresetRow* Row = Preset.DataTable->FindRow<FASLootChestPresetRow>(
			Preset.RowName, TEXT("ResolveLootChestPreset"), false);
		if (!Row) OutError = FString::Printf(TEXT("Loot preset row '%s' does not exist."), *Preset.RowName.ToString());
		return Row;
	}
}

bool UASLootBlueprintLibrary::GenerateLootFromPreset(const FDataTableRowHandle& Preset, const int32 Seed,
	TArray<FASItemGrant>& OutItems, FString& OutError)
{
	OutItems.Reset();
	const FASLootChestPresetRow* Row = ResolvePreset(Preset, OutError);
	if (!Row) return false;
	FRandomStream Random(Seed);
	return FASLootGenerator::Generate(*Row, Random, OutItems, OutError);
}

AASLootChest* UASLootBlueprintLibrary::SpawnLootChestFromPreset(UObject* WorldContextObject,
	const FDataTableRowHandle& Preset, const FTransform& SpawnTransform, const int32 Seed)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_Client || SpawnTransform.ContainsNaN()) return nullptr;
	TArray<FASItemGrant> Items;
	FString Error;
	const FASLootChestPresetRow* Row = ResolvePreset(Preset, Error);
	FRandomStream Random(Seed);
	if (!Row || !FASLootGenerator::Generate(*Row, Random, Items, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootChest] Preset %s: %s"), *Preset.RowName.ToString(), *Error);
		return nullptr;
	}
	return SpawnLootChest(WorldContextObject, Row->ChestClass, SpawnTransform, Items);
}
