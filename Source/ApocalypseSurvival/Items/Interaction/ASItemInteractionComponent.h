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
class UASInventoryPanelWidget;
class UASInventoryViewModel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASInventoryActionFailedSignature, FText, Message);

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
	void ActivateSlotFromKey(int32 SlotIndex);
	void CancelPendingInteraction();
	bool HasPendingInteraction() const { return PendingTarget.IsValid(); }
	bool IsLootOpen() const;
	bool IsCursorOverInventory() const;

	UFUNCTION(BlueprintCallable, Category = "Item|Inventory")
	void ToggleInventory();
	UFUNCTION(BlueprintCallable, Category = "Item|Inventory")
	void CloseInventory();
	UFUNCTION(BlueprintPure, Category = "Item|Inventory")
	bool IsInventoryOpen() const;
	UPROPERTY(BlueprintAssignable, Category = "Item|Inventory")
	FASInventoryActionFailedSignature OnInventoryActionFailed;
	/** Only the nameplate can select a dropped item as a local hover target. */
	void SetHoveredDroppedItemLabel(AASDroppedItem* Item, bool bHovered);

	/** 로컬 플레이어의 바닥 아이템 이름과 줍기를 함께 켜거나 끈다. */
	UFUNCTION(BlueprintCallable, Category = "Item|Drop")
	void ToggleDroppedItemNames();

	UFUNCTION(BlueprintPure, Category = "Item|Drop")
	bool AreDroppedItemNamesVisible() const { return bDroppedItemNamesVisible; }

	UFUNCTION(BlueprintPure, Category = "Item")
	UASInventoryComponent* GetPlayerInventory() const;

	UFUNCTION(BlueprintPure, Category="Item|ViewModel")
	UASInventoryViewModel* GetInventoryViewModel();

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestLoot(AASLootChest* Chest, FGuid InstanceId, int32 Quantity);

	/** Drop the current whole loot stack without requiring a free player-inventory slot. */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void RequestDropLoot(AASLootChest* Chest, FGuid InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Loot|Relic")
	void RequestClaimRelic(AASLootChest* Chest);

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
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryViewModel> InventoryViewModel;
	UFUNCTION(Client, Reliable)
	void ClientInventoryActionFailed(const FText& Message);
	AASPlayerController* GetASController() const;
	bool CanReachTarget(AActor* Target) const;
	void InitializeInventoryBar();
	void UpdateDroppedItemHover();

	UFUNCTION(Server, Reliable)
	void ServerSetDroppedItemNamesVisible(bool bVisible);

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
	void ServerClaimRelic(AASLootChest* Chest);

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
	UPROPERTY()
	TSoftClassPtr<UASLootWidget> DefaultLootWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UASLootWidget> LootWidget;

	/** Designer-authored quick-slot view; defaults to WBP_InventoryBar. */
	UPROPERTY(EditDefaultsOnly, Category = "Item|Inventory")
	TSubclassOf<UASInventoryBarWidget> InventoryBarClass;
	UPROPERTY()
	TSoftClassPtr<UASInventoryBarWidget> DefaultInventoryBarClass;

	UPROPERTY(Transient)
	TObjectPtr<UASInventoryBarWidget> InventoryBar;

	UPROPERTY(EditDefaultsOnly, Category = "Item|Inventory")
	TSubclassOf<UASInventoryPanelWidget> InventoryPanelClass;
	UPROPERTY()
	TSoftClassPtr<UASInventoryPanelWidget> DefaultInventoryPanelClass;
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryPanelWidget> InventoryPanel;

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
	/** 각 플레이어의 로컬 화면 및 해당 플레이어의 서버 줍기 허용 상태. */
	bool bDroppedItemNamesVisible = true;
};
