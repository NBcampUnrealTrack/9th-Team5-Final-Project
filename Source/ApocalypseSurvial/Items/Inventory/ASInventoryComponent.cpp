#include "Items/Inventory/ASInventoryComponent.h"

#include "Items/Data/ASItemDefinition.h"
#include "Items/ASItemInstance.h"
#include "UObject/UObjectGlobals.h"

UASInventoryComponent::UASInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UASInventoryComponent::AddItem(UASItemDefinition* ItemDefinition, const int32 RequestedQuantity)
{
	if (!IsValid(ItemDefinition) || RequestedQuantity <= 0)
	{
		return 0;
	}

	RemoveEmptyAndInvalidInstances();

	int32 RemainingQuantity = RequestedQuantity;

	// Fill existing stacks before consuming additional inventory slots.
	for (UASItemInstance* ItemInstance : Items)
	{
		if (RemainingQuantity <= 0)
		{
			break;
		}

		if (IsValid(ItemInstance) && ItemInstance->GetItemDefinition() == ItemDefinition)
		{
			RemainingQuantity -= ItemInstance->AddQuantity(RemainingQuantity);
		}
	}

	const int32 MaxStackSize = FMath::Max(1, ItemDefinition->MaxStackSize);

	while (RemainingQuantity > 0 && HasFreeSlot())
	{
		const int32 NewInstanceQuantity = FMath::Min(RemainingQuantity, MaxStackSize);
		UASItemInstance* NewItemInstance = NewObject<UASItemInstance>(this);
		NewItemInstance->Initialize(ItemDefinition, NewInstanceQuantity);

		if (!NewItemInstance->IsValidItemInstance())
		{
			break;
		}

		Items.Add(NewItemInstance);
		RemainingQuantity -= NewItemInstance->GetQuantity();
	}

	const int32 AddedQuantity = RequestedQuantity - RemainingQuantity;
	if (AddedQuantity > 0)
	{
		OnInventoryChanged.Broadcast();
	}

	return AddedQuantity;
}

int32 UASInventoryComponent::RemoveItem(UASItemDefinition* ItemDefinition, const int32 RequestedQuantity)
{
	if (!IsValid(ItemDefinition) || RequestedQuantity <= 0)
	{
		return 0;
	}

	int32 RemainingQuantity = RequestedQuantity;

	// Consume from the most recently created matching instance first.
	for (int32 Index = Items.Num() - 1; Index >= 0 && RemainingQuantity > 0; --Index)
	{
		UASItemInstance* ItemInstance = Items[Index];
		if (!IsValid(ItemInstance) || !ItemInstance->IsValidItemInstance())
		{
			Items.RemoveAt(Index);
			continue;
		}

		if (ItemInstance->GetItemDefinition() == ItemDefinition)
		{
			RemainingQuantity -= ItemInstance->RemoveQuantity(RemainingQuantity);

			if (!ItemInstance->IsValidItemInstance())
			{
				Items.RemoveAt(Index);
			}
		}
	}

	const int32 RemovedQuantity = RequestedQuantity - RemainingQuantity;
	if (RemovedQuantity > 0)
	{
		OnInventoryChanged.Broadcast();
	}

	return RemovedQuantity;
}

int32 UASInventoryComponent::RemoveItemInstance(UASItemInstance* ItemInstance, const int32 RequestedQuantity)
{
	if (!IsValid(ItemInstance) || RequestedQuantity <= 0)
	{
		return 0;
	}

	const int32 ItemIndex = Items.IndexOfByKey(ItemInstance);
	if (ItemIndex == INDEX_NONE)
	{
		return 0;
	}

	const int32 RemovedQuantity = ItemInstance->RemoveQuantity(RequestedQuantity);
	if (RemovedQuantity <= 0)
	{
		return 0;
	}

	if (!ItemInstance->IsValidItemInstance())
	{
		Items.RemoveAt(ItemIndex);
	}

	OnInventoryChanged.Broadcast();
	return RemovedQuantity;
}

int32 UASInventoryComponent::TransferItemTo(
	UASInventoryComponent* TargetInventory,
	UASItemInstance* ItemInstance,
	const int32 RequestedQuantity)
{
	if (!IsValid(TargetInventory)
		|| TargetInventory == this
		|| !IsValid(ItemInstance)
		|| RequestedQuantity <= 0
		|| Items.IndexOfByKey(ItemInstance) == INDEX_NONE
		|| !ItemInstance->IsValidItemInstance())
	{
		return 0;
	}

	TargetInventory->RemoveEmptyAndInvalidInstances();

	const int32 QuantityToTransfer = FMath::Min(RequestedQuantity, ItemInstance->GetQuantity());
	int32 RemainingQuantity = QuantityToTransfer;

	// Merge only with compatible instances. CanStackWith must be extended when
	// durability or other per-instance properties affect stacking compatibility.
	for (UASItemInstance* TargetItem : TargetInventory->Items)
	{
		if (RemainingQuantity <= 0)
		{
			break;
		}

		if (IsValid(TargetItem) && TargetItem->CanStackWith(ItemInstance))
		{
			const int32 AddedQuantity = TargetItem->AddQuantity(RemainingQuantity);
			ItemInstance->RemoveQuantity(AddedQuantity);
			RemainingQuantity -= AddedQuantity;
		}
	}

	// A duplicated UObject preserves all current and future UPROPERTY state. A
	// split stack receives a new identity, while a completely moved instance
	// retains its original identity.
	if (RemainingQuantity > 0 && TargetInventory->HasFreeSlot())
	{
		const int32 NewInstanceQuantity = FMath::Min(RemainingQuantity, ItemInstance->GetQuantity());
		UASItemInstance* TransferredInstance = DuplicateObject<UASItemInstance>(ItemInstance, TargetInventory);

		if (IsValid(TransferredInstance))
		{
			const bool bMovedWholeRemainingInstance = NewInstanceQuantity == ItemInstance->GetQuantity();
			TransferredInstance->Quantity = NewInstanceQuantity;

			if (!bMovedWholeRemainingInstance)
			{
				TransferredInstance->InstanceId = FGuid::NewGuid();
			}

			TargetInventory->Items.Add(TransferredInstance);
			ItemInstance->RemoveQuantity(NewInstanceQuantity);
			RemainingQuantity -= NewInstanceQuantity;
		}
	}

	const int32 TransferredQuantity = QuantityToTransfer - RemainingQuantity;
	if (TransferredQuantity <= 0)
	{
		return 0;
	}

	if (!ItemInstance->IsValidItemInstance())
	{
		Items.RemoveSingle(ItemInstance);
	}

	OnInventoryChanged.Broadcast();
	TargetInventory->OnInventoryChanged.Broadcast();
	return TransferredQuantity;
}

int32 UASInventoryComponent::GetItemCount(const UASItemDefinition* ItemDefinition) const
{
	if (!IsValid(ItemDefinition))
	{
		return 0;
	}

	int32 TotalQuantity = 0;
	for (const UASItemInstance* ItemInstance : Items)
	{
		if (IsValid(ItemInstance)
			&& ItemInstance->IsValidItemInstance()
			&& ItemInstance->GetItemDefinition() == ItemDefinition)
		{
			TotalQuantity += ItemInstance->GetQuantity();
		}
	}

	return TotalQuantity;
}

bool UASInventoryComponent::HasItem(const UASItemDefinition* ItemDefinition, const int32 RequiredQuantity) const
{
	return RequiredQuantity > 0 && GetItemCount(ItemDefinition) >= RequiredQuantity;
}

UASItemInstance* UASInventoryComponent::FindFirstItem(const UASItemDefinition* ItemDefinition) const
{
	if (!IsValid(ItemDefinition))
	{
		return nullptr;
	}

	for (UASItemInstance* ItemInstance : Items)
	{
		if (IsValid(ItemInstance)
			&& ItemInstance->IsValidItemInstance()
			&& ItemInstance->GetItemDefinition() == ItemDefinition)
		{
			return ItemInstance;
		}
	}

	return nullptr;
}

TArray<UASItemInstance*> UASInventoryComponent::GetItems() const
{
	TArray<UASItemInstance*> Result;
	Result.Reserve(Items.Num());

	for (UASItemInstance* ItemInstance : Items)
	{
		if (IsValid(ItemInstance) && ItemInstance->IsValidItemInstance())
		{
			Result.Add(ItemInstance);
		}
	}

	return Result;
}

bool UASInventoryComponent::HasFreeSlot() const
{
	return MaxSlots <= 0 || Items.Num() < MaxSlots;
}

void UASInventoryComponent::RemoveEmptyAndInvalidInstances()
{
	Items.RemoveAll([](const TObjectPtr<UASItemInstance>& ItemInstance)
	{
		return !IsValid(ItemInstance) || !ItemInstance->IsValidItemInstance();
	});
}
