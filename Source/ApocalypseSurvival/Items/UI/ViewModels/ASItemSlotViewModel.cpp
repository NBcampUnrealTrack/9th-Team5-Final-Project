#include "Items/UI/ViewModels/ASItemSlotViewModel.h"

#include "Items/UI/ViewModels/ASInventoryViewModel.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Engine/Texture2D.h"

bool FASItemSlotPresentation::operator==(const FASItemSlotPresentation& O) const
{
	return ItemId == O.ItemId && Name.EqualTo(O.Name) && Quantity.EqualTo(O.Quantity) && Key.EqualTo(O.Key)
		&& Equipped.EqualTo(O.Equipped) && Tooltip.EqualTo(O.Tooltip) && Icon == O.Icon && Rarity == O.Rarity
		&& ItemType == O.ItemType && bHasItem == O.bHasItem && bEquipped == O.bEquipped && bCanDrag == O.bCanDrag;
}

void UASItemSlotViewModel::Initialize(UASInventoryViewModel* InInventory, int32 InSlotIndex)
{
	Inventory = InInventory;
	StandaloneItem.Reset();
	SlotIndex = InSlotIndex;
	Refresh();
}

void UASItemSlotViewModel::SetStandaloneItem(UASItemInstance* Item)
{
	Inventory.Reset();
	StandaloneItem = Item;
	SlotIndex = Item ? Item->GetInventorySlot() : INDEX_NONE;
	Refresh();
}

UASItemInstance* UASItemSlotViewModel::ResolveItem() const
{
	UASInventoryComponent* Source = Inventory.IsValid() ? Inventory->GetInventory() : nullptr;
	return Source ? Source->GetItemAtSlot(SlotIndex) : StandaloneItem.Get();
}

void UASItemSlotViewModel::Refresh()
{
	UASItemInstance* Item = ResolveItem();
	const UASItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	if (!IsValid(Definition) || !Item->IsValidItemInstance()) { Item = nullptr; Definition = nullptr; }
	const FSoftObjectPath IconPath = Definition ? Definition->Icon.ToSoftObjectPath() : FSoftObjectPath();
	if (RequestedIcon != IconPath)
	{
		RequestedIcon = IconPath;
		IconRequest.Request(IconPath, FStreamableDelegate::CreateUObject(this, &ThisClass::Refresh));
	}
	FASItemSlotPresentation Value;
	const bool bWeapon = SlotIndex == UASInventoryComponent::WeaponSlotIndex;
	const bool bBag = SlotIndex == UASInventoryComponent::BagSlotIndex;
	Value.Key = bWeapon ? NSLOCTEXT("Items", "WeaponSlot", "무기") : bBag ? NSLOCTEXT("Items", "BagSlot", "가방")
		: Inventory.IsValid() && SlotIndex >= 0 && SlotIndex < UASInventoryComponent::QuickSlotCount
			? Inventory->GetQuickSlotKeyText(SlotIndex) : FText::GetEmpty();
	if (Definition)
	{
		Value.ItemId = Item->GetInstanceId();
		Value.Name = Definition->DisplayName;
		Value.Quantity = Item->GetQuantity() > 1 ? FText::AsNumber(Item->GetQuantity()) : FText::GetEmpty();
		Value.Icon = Definition->Icon.Get();
		Value.Rarity = Definition->Rarity;
		Value.ItemType = Definition->ItemType;
		Value.bHasItem = true;
		Value.bEquipped = bWeapon || bBag;
		Value.bCanDrag = SlotIndex >= 0 || bBag;
		Value.Equipped = Value.bEquipped ? FText::FromString(TEXT("E")) : FText::GetEmpty();
		Value.Tooltip = Definition->ItemType == EASItemType::Bag
			? FText::Format(NSLOCTEXT("Items", "BagCapacityTooltip", "{0}\n장착 시 보관함 +{1}칸"), Definition->DisplayName, FText::AsNumber(Definition->BagAdditionalSlots))
			: Definition->DisplayName;
	}
	UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
}

void UASItemSlotViewModel::Release()
{
	Inventory.Reset();
	StandaloneItem.Reset();
	IconRequest.Reset();
	RequestedIcon.Reset();
	Refresh();
}
