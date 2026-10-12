#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/DataTable.h"
#include "Items/Data/ASItemGrant.h"
#include "ASLootBlueprintLibrary.generated.h"

class AASLootChest;

/** Server-side loot spawning shared by map/spawner code and Blueprint tools. */
UCLASS()
class APOCALYPSESURVIVAL_API UASLootBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Resolve and validate an FASLootChestPresetRow by table and row name; no actor side effects. */
	UFUNCTION(BlueprintCallable, Category = "Items|Loot")
	static bool GenerateLootFromPreset(const FDataTableRowHandle& Preset, int32 Seed,
		TArray<FASItemGrant>& OutItems, FString& OutError);

	/** Server-only. Spawner selects a preset using a table + row name; class and rules come from that row. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot", meta = (WorldContext = "WorldContextObject"))
	static AASLootChest* SpawnLootChestFromPreset(UObject* WorldContextObject,
		const FDataTableRowHandle& Preset, const FTransform& SpawnTransform, int32 Seed);
	/** Spawn a chest with explicit contents replacing its defaults. Empty Items means an empty chest.
	 * Uses deferred spawning to set contents before BeginPlay. Returns null on invalid input or spawn failure.
	 * ChestClass can be BP_LootChest; collision uses AdjustIfPossibleButDontSpawnIfColliding.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot",
		meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ChestClass"))
	static AASLootChest* SpawnLootChest(UObject* WorldContextObject, TSubclassOf<AASLootChest> ChestClass,
		const FTransform& SpawnTransform, const TArray<FASItemGrant>& Items);
};
