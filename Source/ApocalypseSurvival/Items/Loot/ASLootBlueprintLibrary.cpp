#include "Items/Loot/ASLootBlueprintLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/Loot/ASLootChest.h"
#include "Items/Loot/ASLootDefinition.h"
#include "Items/Loot/ASLootGenerator.h"
#include "Items/Loot/ASLootValidation.h"
#include "Items/Loot/ASLootSettings.h"
#include "Items/ASItemInstance.h"
#include "Items/Inventory/ASInventoryComponent.h"

bool FASLootChestSpawnResult::IsSuccess() const
{
	return Status == EASLootChestSpawnStatus::Success && IsValid(Chest.Get());
}

namespace
{
	FASLootChestSpawnResult Failure(const EASLootChestSpawnStatus Status, const FString& Error)
	{
		FASLootChestSpawnResult Result;
		Result.Status = Status;
		Result.ErrorMessage = Error;
		return Result;
	}

	UWorld* ValidateContext(UObject* Context, const FTransform& Transform, FASLootChestSpawnResult& OutFailure)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
		if (!World) { OutFailure = Failure(EASLootChestSpawnStatus::InvalidWorld, TEXT("World context is invalid.")); return nullptr; }
		if (World->GetNetMode() == NM_Client)
		{
			OutFailure = Failure(EASLootChestSpawnStatus::NotAuthority, TEXT("Loot chests can only be spawned on the server."));
			return nullptr;
		}
		if (Transform.ContainsNaN())
		{
			OutFailure = Failure(EASLootChestSpawnStatus::InvalidTransform, TEXT("Spawn transform contains a non-finite value."));
			return nullptr;
		}
		return World;
	}

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

	const FASLootRarityAppearanceRow* ResolveRarityAppearance(const EASLootRarity Rarity)
	{
		const UASLootSettings* Settings = GetDefault<UASLootSettings>();
		UDataTable* Table = Settings ? Settings->RarityAppearanceTable.LoadSynchronous() : nullptr;
		if (!Table || Table->GetRowStruct() != FASLootRarityAppearanceRow::StaticStruct()) return nullptr;
		const FString RowName = StaticEnum<EASLootRarity>()->GetNameStringByValue(static_cast<int64>(Rarity));
		const FASLootRarityAppearanceRow* Row = Table->FindRow<FASLootRarityAppearanceRow>(FName(*RowName),
			TEXT("ResolveLootRarityAppearance"), false);
		// Enum redirects migrate the value, while existing DataTable row names remain unchanged.
		if (!Row && Rarity == EASLootRarity::Uncommon)
		{
			Row = Table->FindRow<FASLootRarityAppearanceRow>(TEXT("Common"), TEXT("ResolveLootRarityAppearance"), false);
		}
		return Row && Row->Rarity == Rarity ? Row : nullptr;
	}
}

bool UASLootBlueprintLibrary::ValidateLootItems(const TArray<FASItemGrant>& Items, FString& OutError)
{
	return FASLootValidation::ValidateGrants(Items, OutError);
}

FASLootChestSpawnResult UASLootBlueprintLibrary::SpawnLootChestWithResult(UObject* WorldContextObject,
	TSubclassOf<AASLootChest> ChestClass, const FTransform& SpawnTransform, const TArray<FASItemGrant>& Items)
{
	return SpawnLootChestWithAppearance(WorldContextObject, ChestClass, SpawnTransform, Items, nullptr);
}

FASLootChestSpawnResult UASLootBlueprintLibrary::SpawnLootChestWithAppearance(UObject* WorldContextObject,
	TSubclassOf<AASLootChest> ChestClass, const FTransform& SpawnTransform,
	const TArray<FASItemGrant>& Items, const FASLootChestPresetRow* Preset)
{
	FASLootChestSpawnResult Result;
	UWorld* World = ValidateContext(WorldContextObject, SpawnTransform, Result);
	if (!World) return Result;
	if (!ChestClass || ChestClass->HasAnyClassFlags(CLASS_Abstract))
		return Failure(EASLootChestSpawnStatus::InvalidChestClass, TEXT("A concrete loot chest class is required."));
	FString Error;
	if (!FASLootValidation::ValidateGrants(Items, Error))
		return Failure(EASLootChestSpawnStatus::InvalidItems, Error);

	AASLootChest* Chest = World->SpawnActorDeferred<AASLootChest>(ChestClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Chest) return Failure(EASLootChestSpawnStatus::SpawnFailed, TEXT("Could not create the chest at the requested transform."));
	if (!Chest->SetInitialLoot(Items))
	{
		Chest->Destroy();
		return Failure(EASLootChestSpawnStatus::InventoryInitializationFailed, TEXT("Chest refused the initial loot configuration."));
	}
	if (Preset)
	{
		const FASLootRarityAppearanceRow* Appearance = ResolveRarityAppearance(Preset->Rarity);
		Chest->SetLootRarityAppearance(Preset->Rarity,
			Appearance ? Appearance->ChestMesh.Get() : nullptr,
			Appearance ? Appearance->ChestMaterial.Get() : nullptr,
			Appearance ? Appearance->MaterialIndex : 0,
			Appearance ? Appearance->GetResolvedColor() : FLinearColor::White,
			Appearance ? Appearance->ColorParameter : NAME_None);
	}
	Chest->FinishSpawning(SpawnTransform);
	if (!IsValid(Chest) || Chest->IsActorBeingDestroyed())
	{
		Error = Chest->GetInitialLootError();
		return Failure(Error.IsEmpty() ? EASLootChestSpawnStatus::SpawnFailed : EASLootChestSpawnStatus::InventoryInitializationFailed,
			Error.IsEmpty() ? TEXT("Chest was destroyed while finishing its spawn.") : Error);
	}
	if (Preset)
	{
		// Construction scripts may set their default mesh/material after the deferred spawn setup.
		const FASLootRarityAppearanceRow* Appearance = ResolveRarityAppearance(Preset->Rarity);
		Chest->SetLootRarityAppearance(Preset->Rarity,
			Appearance ? Appearance->ChestMesh.Get() : nullptr,
			Appearance ? Appearance->ChestMaterial.Get() : nullptr,
			Appearance ? Appearance->MaterialIndex : 0,
			Appearance ? Appearance->GetResolvedColor() : FLinearColor::White,
			Appearance ? Appearance->ColorParameter : NAME_None);
	}
	// Covers worlds that have not dispatched BeginPlay yet. BeginPlay will reuse this result.
	if (!Chest->InitializeInitialLoot(Error))
	{
		Chest->Destroy();
		return Failure(EASLootChestSpawnStatus::InventoryInitializationFailed, Error);
	}
	if (!IsValid(Chest) || Chest->IsActorBeingDestroyed())
		return Failure(EASLootChestSpawnStatus::SpawnFailed, TEXT("Chest was destroyed during inventory initialization."));
	// Construction/BeginPlay hooks must not silently change the API's requested contents.
	TMap<const UASItemDefinition*, int64> ExpectedTotals;
	for (const FASItemGrant& Grant : Items) ExpectedTotals.FindOrAdd(Grant.ItemDefinition) += Grant.Quantity;
	UASInventoryComponent* Inventory = Chest->GetLootInventory();
	bool bContentsMatch = IsValid(Inventory);
	if (bContentsMatch)
	{
		for (const auto& Total : ExpectedTotals)
			bContentsMatch &= Inventory->GetItemCount(Total.Key) == Total.Value;
		for (const UASItemInstance* Item : Inventory->GetItems())
			bContentsMatch &= IsValid(Item) && ExpectedTotals.Contains(Item->GetItemDefinition());
	}
	if (!bContentsMatch)
	{
		Chest->Destroy();
		return Failure(EASLootChestSpawnStatus::InventoryInitializationFailed,
			TEXT("Chest contents changed during spawn; the incomplete chest was discarded."));
	}
	Result.Chest = Chest;
	Result.Status = EASLootChestSpawnStatus::Success;
	return Result;
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

FASLootChestSpawnResult UASLootBlueprintLibrary::SpawnLootChestFromPresetWithResult(UObject* WorldContextObject,
	const FDataTableRowHandle& Preset, const FTransform& SpawnTransform, const int32 Seed)
{
	FASLootChestSpawnResult Result;
	if (!ValidateContext(WorldContextObject, SpawnTransform, Result)) return Result;
	FString Error;
	const FASLootChestPresetRow* Row = ResolvePreset(Preset, Error);
	if (!Row || !Row->Validate(Error)) return Failure(EASLootChestSpawnStatus::InvalidPreset, Error);
	TArray<FASItemGrant> Items;
	FRandomStream Random(Seed);
	if (!FASLootGenerator::Generate(*Row, Random, Items, Error))
		return Failure(EASLootChestSpawnStatus::LootGenerationFailed, Error);
	return SpawnLootChestWithAppearance(WorldContextObject, Row->ChestClass, SpawnTransform, Items, Row);
}
