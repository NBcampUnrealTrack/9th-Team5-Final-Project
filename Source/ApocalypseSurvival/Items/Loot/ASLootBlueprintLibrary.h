#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/DataTable.h"
#include "Items/Data/ASItemGrant.h"
#include "ASLootBlueprintLibrary.generated.h"

class AASLootChest;

UENUM(BlueprintType)
enum class EASLootChestSpawnStatus : uint8
{
	Success,
	InvalidWorld,
	NotAuthority,
	InvalidTransform,
	InvalidChestClass,
	InvalidPreset,
	LootGenerationFailed,
	InvalidItems,
	SpawnFailed,
	InventoryInitializationFailed
};

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASLootChestSpawnResult
{
	GENERATED_BODY()
	/** Non-null only when spawning and full initial inventory initialization succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	TObjectPtr<AASLootChest> Chest = nullptr;
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	EASLootChestSpawnStatus Status = EASLootChestSpawnStatus::SpawnFailed;
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	FString ErrorMessage;
	bool IsSuccess() const;
};

/** Server-side loot spawning shared by map/spawner code and Blueprint tools. */
UCLASS()
class APOCALYPSESURVIVAL_API UASLootBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot", meta = (WorldContext = "WorldContextObject"))
	static FASLootChestSpawnResult SpawnLootChestWithResult(UObject* WorldContextObject,
		TSubclassOf<AASLootChest> ChestClass, const FTransform& SpawnTransform, const TArray<FASItemGrant>& Items);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot", meta = (WorldContext = "WorldContextObject"))
	static FASLootChestSpawnResult SpawnLootChestFromPresetWithResult(UObject* WorldContextObject,
		const FDataTableRowHandle& Preset, const FTransform& SpawnTransform, int32 Seed);

	/** Shared grant validation without spawning. Empty Items is valid and means an empty chest. */
	UFUNCTION(BlueprintCallable, Category = "Items|Loot")
	static bool ValidateLootItems(const TArray<FASItemGrant>& Items, FString& OutError);
	/** Resolve and validate an FASLootChestPresetRow by table and row name; no actor side effects. */
	UFUNCTION(BlueprintCallable, Category = "Items|Loot")
	static bool GenerateLootFromPreset(const FDataTableRowHandle& Preset, int32 Seed,
		TArray<FASItemGrant>& OutItems, FString& OutError);
};
