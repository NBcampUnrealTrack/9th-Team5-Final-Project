#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "ASInventoryComponent.generated.h"

class UASItemDefinition;
class UASItemInstance;
class UAbilitySystemComponent;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASInventoryChangedSignature);

/**
 * Owns and manages the runtime item instances held by an actor.
 *
 * A MaxSlots value of zero means the inventory has no slot limit. Mutations
 * should go through this component so stacking rules and change notifications
 * remain consistent.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIAL_API UASInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASInventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UASItemInstance* FindItemById(FGuid InstanceId) const;

	/** Authority-only gameplay operations. Clients request these through ASPlayerController. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|GAS")
	bool UseItem(FGuid InstanceId, APawn* Avatar);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|GAS")
	bool EquipItem(FGuid InstanceId, APawn* Avatar);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|GAS")
	void UnequipItem();

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	UASItemInstance* GetEquippedItem() const { return EquippedItem; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	AActor* GetEquippedActor() const { return EquippedActor; }

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

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UASItemInstance* FindFirstItem(const UASItemDefinition* ItemDefinition) const;

	/** Returns a Blueprint-friendly snapshot of the current instance references. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UASItemInstance*> GetItems() const;

	/** Returns the internal array for read-only C++ iteration. */
	const TArray<TObjectPtr<UASItemInstance>>& GetItemInstances() const { return Items; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetUsedSlotCount() const { return Items.Num(); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetMaxSlots() const { return MaxSlots; }

	/** Fired once after a successful public mutation. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FASInventoryChangedSignature OnInventoryChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool HasInventoryAuthority() const;
	UAbilitySystemComponent* ResolveAbilitySystem(APawn* Avatar) const;
	void NotifyInventoryChanged();
	void SyncReplicatedItems();
	void ClearEquipment();

	UFUNCTION()
	void OnRep_Items();

	bool HasFreeSlot() const;
	void RemoveEmptyAndInvalidInstances();

	/** Maximum number of item instances. Zero means unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 MaxSlots = 0;

	/** Runtime instances owned by this component. */
	UPROPERTY(ReplicatedUsing = OnRep_Items, VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UASItemInstance>> Items;

	UPROPERTY(ReplicatedUsing = OnRep_Items)
	TObjectPtr<UASItemInstance> EquippedItem;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> EquippedActor;

	// Strong references keep removed subobjects alive until they are unregistered.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UASItemInstance>> RegisteredItems;

	TWeakObjectPtr<UAbilitySystemComponent> EquippedAbilitySystem;
	TArray<FGameplayAbilitySpecHandle> EquippedAbilityHandles;
};
