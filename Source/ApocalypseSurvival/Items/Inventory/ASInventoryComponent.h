#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Items/Data/ASItemAssetLoading.h"
#include "Items/Data/ASItemGrant.h"
#include "Items/Inventory/ASInventoryExchangeTypes.h"
#include "ASInventoryComponent.generated.h"

class UASItemDefinition;
class UASItemInstance;
class APawn;
class UASItemUseComponent;
class AASDroppedItem;
class UASLoadingFlowSubsystem;
struct FASInventoryMutationPlan;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASInventoryChangedSignature);

/**
 * Owns and manages the runtime item instances held by an actor.
 *
 * A MaxSlots value of zero means the inventory has no slot limit. Mutations
 * should go through this component so stacking rules and change notifications
 * remain consistent.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 QuickSlotCount = 10;
	static constexpr int32 BaseStorageSlotCount = 16;
	/** Legacy name retained for existing quick-slot callers. Not the total inventory capacity. */
	static constexpr int32 PlayerSlotCount = QuickSlotCount;
	/** Dedicated weapon position; owned instances here remain replicated but do not use backpack capacity. */
	static constexpr int32 WeaponSlotIndex = -2;
	static constexpr int32 BagSlotIndex = -3;

	UASInventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;

	/** Server-only, all-or-nothing grant. Success is remembered for this component's lifetime. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Initial Items")
	bool GrantInitialItems(FString& OutError);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UASItemInstance* FindItemById(FGuid InstanceId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UASItemInstance* GetItemAtSlot(int32 SlotIndex) const;

	/** Merge matching stackable items into ToSlot; otherwise swap positions. Reject stale item identities. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool SwapSlots(int32 FromSlot, int32 ToSlot, FGuid ExpectedFromItem, FGuid ExpectedToItem);

	/** Drop the entire current stack near the owning player's avatar. Returns null when refused. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	AASDroppedItem* DropSlot(int32 SlotIndex, FGuid ExpectedItem, APawn* Avatar);

	/** Drop exactly one unit from a backpack slot. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	AASDroppedItem* DropOneFromSlot(int32 SlotIndex, FGuid ExpectedItem, APawn* Avatar);

	/** Authority-only request. True also accepts one pending async use; clients go through ASPlayerController. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|GAS")
	bool UseItem(FGuid InstanceId, APawn* Avatar);
	/** Refreshes the inventory's asynchronous asset cache on authority and replicated clients. */
	void PreloadItemAssets();

	/** Entire quantity or no mutation. Server-only, including when called from C++. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryConsumeItem(UASItemDefinition* ItemDefinition, int32 Quantity);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryConsumeItemInstance(UASItemInstance* ItemInstance, int32 Quantity);

	/** True accepts an async equip request or completes immediately. Equipment owns actual equipping. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|GAS")
	bool EquipItem(FGuid InstanceId, APawn* Avatar);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|GAS")
	void UnequipItem();

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	UASItemInstance* GetEquippedItem() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	AActor* GetEquippedActor() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Bag")
	UASItemInstance* GetEquippedBag() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Bag")
	bool EquipBag(FGuid InstanceId, APawn* Avatar);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Bag")
	bool UnequipBag(APawn* Avatar, int32 PreferredSlot = -1);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsValidItemSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetStorageSlotCount() const;

	/**
	 * Adds items using the supplied definition.
	 * @return The quantity actually added. This can be less than RequestedQuantity.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(UASItemDefinition* ItemDefinition, int32 RequestedQuantity = 1);

	/**
	 * Removes matching items across their instances.
	 * @return The quantity actually removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItem(UASItemDefinition* ItemDefinition, int32 RequestedQuantity = 1);

	/**
	 * Removes quantity from one specific runtime instance.
	 * @return The quantity actually removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItemInstance(UASItemInstance* ItemInstance, int32 RequestedQuantity = 1);

	/**
	 * Moves as much of an owned instance as possible to another inventory.
	 * Instance properties are copied when a stack must be split.
	 * @return The quantity actually transferred.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 TransferItemTo(
		UASInventoryComponent* TargetInventory,
		UASItemInstance* ItemInstance,
		int32 RequestedQuantity);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(const UASItemDefinition* ItemDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(const UASItemDefinition* ItemDefinition, int32 RequiredQuantity = 1) const;

	/** Read-only preview using backpack contents, including slots freed by consuming the inputs. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Exchange")
	EASInventoryExchangeResult CanExchangeItems(const TArray<FASItemGrant>& Costs, const TArray<FASItemGrant>& Rewards) const;

	/** Server-only all-or-nothing exchange. No callbacks or live mutations occur until the plan succeeds. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Exchange")
	EASInventoryExchangeResult TryExchangeItems(const TArray<FASItemGrant>& Costs, const TArray<FASItemGrant>& Rewards);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UASItemInstance* FindFirstItem(const UASItemDefinition* ItemDefinition) const;

	/** Returns all owned instances, including the weapon position. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UASItemInstance*> GetItems() const;

	/** Returns the internal array for read-only C++ iteration. */
	const TArray<TObjectPtr<UASItemInstance>>& GetItemInstances() const { return Items; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetUsedSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetMaxSlots() const;

	/** Fired once after a successful public mutation. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FASInventoryChangedSignature OnInventoryChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool IsPlayerInventory() const;
	bool ChangeEquippedBag(UASItemInstance* NewBag, APawn* Avatar, int32 PreferredReturnSlot = INDEX_NONE);
	int32 GetBackpackSlotLimit() const;
	/** Apply a freshly built plan without broadcasting. Callers publish only after setting their own state. */
	bool ApplyMutationPlan(const FASInventoryMutationPlan& Plan);
	AASDroppedItem* DropSlotInternal(int32 SlotIndex, FGuid ExpectedItem, APawn* Avatar, bool bSingleItem);
	APawn* GetInventoryAvatar() const;
	UActorComponent* GetEquipmentProvider() const;
	bool HasInventoryAuthority() const;
	void NotifyInventoryChanged();
	void SyncReplicatedItems();
	bool PrepareItemAction(FGuid InstanceId, APawn* Avatar, bool bEquipOnly, bool& bOutQueued);
	void CancelPendingItemAction();

	UFUNCTION()
	void OnRep_Items();
	UFUNCTION()
	void HandleLoadingStarted();

	bool HasFreeSlot() const;
	int32 FindFreeSlotIndex() const;
	void RemoveEmptyAndInvalidInstances();

	/** Container limit; zero is unlimited. Player capacity is always 10 quick + 16 base + equipped bag bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 MaxSlots = 0;

	/** Optional starting-items table row. Automatically granted on the server at BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Initial Items",
		meta = (AllowPrivateAccess = "true", RowType = "/Script/ApocalypseSurvival.ASStartingItemsRow"))
	FDataTableRowHandle InitialItemsPreset;

	/** All owned instances. InventorySlot identifies a backpack position or WeaponSlotIndex. */
	UPROPERTY(ReplicatedUsing = OnRep_Items, VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UASItemInstance>> Items;

	// Strong references keep removed subobjects alive until they are unregistered.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UASItemInstance>> RegisteredItems;

	UPROPERTY(Transient)
	TObjectPtr<UASItemUseComponent> ItemUseComponent;

	bool bChangingEquipment = false;
	bool bInitialItemsGranted = false;
	TWeakObjectPtr<UASLoadingFlowSubsystem> LoadingFlow;
	FASItemAssetRequest InventoryAssetRequest;
	FASItemAssetRequest ActionAssetRequest;
	uint64 PendingActionSerial = 0;
	FGuid PendingActionItem;
	TWeakObjectPtr<APawn> PendingActionAvatar;
	bool bPendingEquipOnly = false;
};
