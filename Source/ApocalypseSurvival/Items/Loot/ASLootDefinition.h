#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Items/Data/ASItemGrant.h"
#include "Items/Data/ASItemTypes.h"
#include "ASLootDefinition.generated.h"

class AASLootChest;
class UASItemDefinition;
class UMaterialInterface;
class UStaticMesh;

/** Shared visual settings resolved by rarity from the configured appearance DataTable. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASLootRarityAppearanceRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	EASLootRarity Rarity = EASLootRarity::Uncommon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UStaticMesh> ChestMesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UMaterialInterface> ChestMaterial = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0"))
	int32 MaterialIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Color")
	FName ColorParameter = TEXT("RarityColor");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Color")
	bool bOverrideColor = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Color",
		meta = (EditCondition = "bOverrideColor", EditConditionHides))
	FLinearColor ColorOverride = FLinearColor::White;

	FLinearColor GetResolvedColor() const;
	bool Validate(FString& OutError) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** Candidate table row: the single source of item weights and random quantity ranges. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASLootPoolEntry : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UASItemDefinition> ItemDefinition = nullptr;
	/** Relative selection weight; zero disables this candidate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinQuantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxQuantity = 1;

	bool Validate(FString& OutError) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASLootTagWeight
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (Categories = "Item"))
	FGameplayTag ItemTag;
	/** Multiplied when ItemTags.HasTag(ItemTag) matches, including child tags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float Multiplier = 1.0f;
};

/** One chest preset per row, directly referencing an FASLootPoolEntry candidate table. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASLootChestPresetRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	EASLootRarity Rarity = EASLootRarity::Uncommon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<AASLootChest> ChestClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Fixed")
	TArray<FASItemGrant> FixedItems;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/ApocalypseSurvival.ASLootPoolEntry"))
	TObjectPtr<UDataTable> RandomPool;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random", meta = (ClampMin = "0", ClampMax = "10000"))
	int32 MinRollCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random", meta = (ClampMin = "0", ClampMax = "10000"))
	int32 MaxRollCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random")
	TMap<EASItemType, float> CategoryWeights;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random")
	TArray<FASLootTagWeight> TagWeights;
	/** False excludes fixed and previously selected items; stops when candidates are exhausted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random")
	bool bAllowRepeatedItems = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bMergeIdenticalItems = true;

	/** Resolves candidate rows in name order and rejects duplicate ItemIds or invalid entries. */
	bool ResolveRandomEntries(TArray<FASLootPoolEntry>& OutEntries, FString& OutError) const;
	bool Validate(FString& OutError) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
