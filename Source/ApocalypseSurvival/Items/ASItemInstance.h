#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Items/Data/ASItemTypes.h"
#include "ASItemInstance.generated.h"

class UASItemDefinition;
class UASInventoryComponent;

/**
 * Mutable runtime state for an item owned by an inventory or loot container.
 *
 * The referenced item definition contains shared, designer-authored data. State
 * that can differ per acquired item belongs here (quantity now, durability or
 * rolled attributes later).
 */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASItemInstance : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UWorld* GetWorld() const override;
	virtual void PostNetReceive() override;

	/** Initializes a newly created instance. Intended to be called by its owning container. */
	void Initialize(UASItemDefinition* InItemDefinition, int32 InQuantity = 1);

	/** Returns whether this instance references an item and contains at least one unit. */
	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsValidItemInstance() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	UASItemDefinition* GetItemDefinition() const { return ItemDefinition; }

	/** Reads the shared definition's rarity on both server and clients. */
	UFUNCTION(BlueprintPure, Category = "Item")
	EASLootRarity GetRarity() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetQuantity() const { return Quantity; }

	UFUNCTION(BlueprintPure, Category = "Item")
	FGuid GetInstanceId() const { return InstanceId; }

	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetInventorySlot() const { return InventorySlot; }

	/** Returns how many additional units can be added without exceeding MaxStackSize. */
	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetRemainingStackSpace() const;

	/** Returns whether the two instances can currently be merged into one stack. */
	UFUNCTION(BlueprintPure, Category = "Item")
	bool CanStackWith(const UASItemInstance* Other) const;

	/** Adds as much as possible and returns the amount actually added. */
	int32 AddQuantity(int32 RequestedQuantity);

	/** Removes as much as possible and returns the amount actually removed. */
	int32 RemoveQuantity(int32 RequestedQuantity);

private:
	friend class UASInventoryComponent;

	/** Stable identity for this runtime item instance. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
	FGuid InstanceId;

	/** Shared, immutable definition of this item. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASItemDefinition> ItemDefinition;

	/** Number of identical units represented by this instance. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 Quantity = 0;

	UPROPERTY(Replicated)
	int32 InventorySlot = INDEX_NONE;
};
