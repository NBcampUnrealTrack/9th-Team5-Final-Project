#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASItemInteractionComponent.generated.h"

class AASLootChest;
class AASDroppedItem;
class AASPlayerController;
class UASInventoryComponent;
class UASLootWidget;
class UASInventoryBarWidget;
class UInputComponent;

/** Controller-owned RPC bridge. Finds an existing player inventory; never creates one. */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASItemInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASItemInteractionComponent();
	bool TryInteractWith(AActor* Target);
	bool RequestPickupDroppedItem(AASDroppedItem* Item);
	void HandleWorldRightClick();
	void CancelPendingInteraction();
	bool HasPendingInteraction() const { return PendingTarget.IsValid(); }
	bool IsLootOpen() const;
	bool IsCursorOverInventory() const;
	/** Prefer the nameplate over the world underneath it when choosing a local hover target. */
	void SetHoveredDroppedItemLabel(AASDroppedItem* Item, bool bHovered);

	UFUNCTION(BlueprintPure, Category = "Item")
	UASInventoryComponent* GetPlayerInventory() const;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestLoot(AASLootChest* Chest, FGuid InstanceId, int32 Quantity);

	/** Drop the current whole loot stack without requiring a free player-inventory slot. */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void RequestDropLoot(AASLootChest* Chest, FGuid InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestUseItem(FGuid InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestEquipItem(FGuid InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestUnequipItem();

	UFUNCTION(BlueprintCallable, Category = "Item|Inventory")
	void RequestActivateSlot(int32 SlotIndex, FGuid ExpectedItem);

	UFUNCTION(BlueprintCallable, Category = "Item|Inventory")
	void RequestSwapSlots(int32 FromSlot, int32 ToSlot, FGuid ExpectedFrom, FGuid ExpectedTo);

	UFUNCTION(BlueprintCallable, Category = "Item|Inventory")
	void RequestDropSlot(int32 SlotIndex, FGuid ExpectedItem);

	UFUNCTION(BlueprintCallable, Category = "Item|Inventory")
	void RequestDropOneFromSlot(int32 SlotIndex, FGuid ExpectedItem);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void CloseLoot();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AASPlayerController* GetASController() const;
	bool CanReachTarget(AActor* Target) const;
	void InitializeInventoryBar();
	void ActivateSlotFromKey(int32 SlotIndex);
	void UpdateDroppedItemHover();

	UFUNCTION(Server, Reliable)
	void ServerActivateSlot(int32 SlotIndex, FGuid ExpectedItem);

	UFUNCTION(Server, Reliable)
	void ServerSwapSlots(int32 FromSlot, int32 ToSlot, FGuid ExpectedFrom, FGuid ExpectedTo);

	UFUNCTION(Server, Reliable)
	void ServerDropSlot(int32 SlotIndex, FGuid ExpectedItem);

	UFUNCTION(Server, Reliable)
	void ServerDropOneFromSlot(int32 SlotIndex, FGuid ExpectedItem);

	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerTakeLoot(AASLootChest* Chest, FGuid InstanceId, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void ServerDropLoot(AASLootChest* Chest, FGuid InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerUseItem(FGuid InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerEquipItem(FGuid InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerUnequipItem();

	UFUNCTION(Server, Reliable)
	void ServerCloseLoot();

	UFUNCTION(Client, Reliable)
	void ClientOpenLoot(AASLootChest* Chest);

	UFUNCTION()
	void HandleLootClosed();

	UPROPERTY(EditDefaultsOnly, Category = "Loot")
	TSubclassOf<UASLootWidget> LootWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UASLootWidget> LootWidget;

	/** Optional visual override. The native class works without any BP assets. */
	UPROPERTY(EditDefaultsOnly, Category = "Item|Inventory")
	TSubclassOf<UASInventoryBarWidget> InventoryBarClass;

	UPROPERTY(Transient)
	TObjectPtr<UASInventoryBarWidget> InventoryBar;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> InventoryInput;

	TWeakObjectPtr<AActor> PendingTarget;
	TWeakObjectPtr<AASDroppedItem> HoveredDroppedItem;
	TWeakObjectPtr<AASDroppedItem> HoveredDroppedItemLabel;
	TWeakObjectPtr<AASLootChest> OpenChest;
	TWeakObjectPtr<AASLootChest> ServerOpenChest;
	TWeakObjectPtr<APawn> ServerLootPawn;
	TWeakObjectPtr<APawn> LootPawn;
	double PendingStartedAt = 0.0;
};
