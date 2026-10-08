#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ASItemEquipmentInterface.generated.h"

class APawn;
class UASInventoryComponent;
class UASItemInstance;

/** Implement on the team's ItemEquipmentComponent attached to the current player pawn. */
UINTERFACE(BlueprintType)
class APOCALYPSESURVIVAL_API UASItemEquipmentInterface : public UInterface
{
	GENERATED_BODY()
};

class APOCALYPSESURVIVAL_API IASItemEquipmentInterface
{
	GENERATED_BODY()
public:
	/** Called only on the server after inventory ownership, avatar and weapon type validation. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item|Equipment")
	bool EquipInventoryItem(UASInventoryComponent* Inventory, UASItemInstance* Item, APawn* Avatar);

	/** Return true only after clearing equipment for this inventory. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item|Equipment")
	bool UnequipInventoryItem(UASInventoryComponent* Inventory, APawn* Avatar);

	/** Return replicated equipment state. The component owns this reference, not inventory. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item|Equipment")
	UASItemInstance* GetEquippedInventoryItem(const UASInventoryComponent* Inventory) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item|Equipment")
	AActor* GetEquippedInventoryActor(const UASInventoryComponent* Inventory) const;
};
