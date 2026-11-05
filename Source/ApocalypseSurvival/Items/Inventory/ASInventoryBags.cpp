#include "Items/Inventory/ASInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ASAbilityTags.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"

bool UASInventoryComponent::IsPlayerInventory() const
{
	return Cast<APawn>(GetOwner()) || Cast<AController>(GetOwner()) || Cast<APlayerState>(GetOwner());
}

UASItemInstance* UASInventoryComponent::GetEquippedBag() const
{
	for (UASItemInstance* Item : Items)
		if (IsValid(Item) && Item->IsValidItemInstance() && Item->GetInventorySlot() == BagSlotIndex
			&& Item->GetItemDefinition()->ItemType == EASItemType::Bag) return Item;
	return nullptr;
}

int32 UASInventoryComponent::GetMaxSlots() const
{
	if (!IsPlayerInventory()) return MaxSlots;
	const UASItemInstance* Bag = GetEquippedBag();
	const int32 Bonus = Bag ? FMath::Clamp(Bag->GetItemDefinition()->BagAdditionalSlots, 0, 256) : 0;
	return QuickSlotCount + BaseStorageSlotCount + Bonus;
}

int32 UASInventoryComponent::GetStorageSlotCount() const
{
	return IsPlayerInventory() ? GetMaxSlots() - QuickSlotCount : GetMaxSlots();
}

bool UASInventoryComponent::IsValidItemSlot(int32 SlotIndex) const
{
	return SlotIndex >= 0 && (GetMaxSlots() <= 0 || SlotIndex < GetMaxSlots());
}

bool UASInventoryComponent::EquipBag(FGuid InstanceId, APawn* Avatar)
{
	UASItemInstance* Bag = FindItemById(InstanceId);
	if (!Bag || Bag == GetEquippedBag() || Bag->InventorySlot < 0
		|| Bag->GetItemDefinition()->ItemType != EASItemType::Bag || Bag->GetQuantity() != 1
		|| Bag->GetItemDefinition()->MaxStackSize != 1 || !IsValidLootRarity(Bag->GetRarity())
		|| Bag->GetItemDefinition()->BagAdditionalSlots < 0 || Bag->GetItemDefinition()->BagAdditionalSlots > 256) return false;
	return ChangeEquippedBag(Bag, Avatar);
}

bool UASInventoryComponent::UnequipBag(APawn* Avatar, int32 PreferredSlot)
{
	return GetEquippedBag() && ChangeEquippedBag(nullptr, Avatar, PreferredSlot);
}

bool UASInventoryComponent::ChangeEquippedBag(UASItemInstance* NewBag, APawn* Avatar, int32 PreferredReturnSlot)
{
	if (!HasInventoryAuthority() || !IsPlayerInventory() || !IsValid(Avatar) || Avatar != GetInventoryAvatar()) return false;
	const UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(this, Avatar);
	if (!ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)) return false;
	UASItemInstance* Previous = GetEquippedBag();
	const int32 NewLimit = QuickSlotCount + BaseStorageSlotCount
		+ (NewBag ? NewBag->GetItemDefinition()->BagAdditionalSlots : 0);
	if (PreferredReturnSlot != INDEX_NONE && (PreferredReturnSlot < 0 || PreferredReturnSlot >= NewLimit
		|| GetItemAtSlot(PreferredReturnSlot))) return false;

	// Reserve surviving positions, then repack items from disappearing slots into available holes.
	// The old bag also needs an ordinary slot. Refuse the entire change if anything would be lost.
	TMap<UASItemInstance*, int32> Positions;
	TArray<UASItemInstance*> Relocate;
	TSet<int32> Occupied;
	for (UASItemInstance* Item : Items)
	{
		if (!IsValid(Item) || !Item->IsValidItemInstance() || Item == NewBag) continue;
		int32 Slot = Item->InventorySlot;
		if (Item == Previous)
			Slot = PreferredReturnSlot != INDEX_NONE ? PreferredReturnSlot : (NewBag ? NewBag->InventorySlot : INDEX_NONE);
		else if (Slot < 0) continue; // Weapon equipment is independent of bag capacity.
		if (Slot >= 0 && Slot < NewLimit && !Occupied.Contains(Slot))
		{
			Positions.Add(Item, Slot);
			Occupied.Add(Slot);
		}
		else Relocate.Add(Item);
	}
	for (UASItemInstance* Item : Relocate)
	{
		int32 Slot = 0;
		while (Occupied.Contains(Slot)) ++Slot;
		if (Slot >= NewLimit) return false;
		Positions.Add(Item, Slot);
		Occupied.Add(Slot);
	}
	for (const auto& Position : Positions) Position.Key->InventorySlot = Position.Value;
	if (NewBag) NewBag->InventorySlot = BagSlotIndex;
	NotifyInventoryChanged();
	return true;
}
