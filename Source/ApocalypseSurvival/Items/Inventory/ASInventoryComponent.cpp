#include "Items/Inventory/ASInventoryComponent.h"

#include "Items/Data/ASItemDefinition.h"
#include "Items/ASItemInstance.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Core/Loading/ASLoadingFlowSubsystem.h"


#include "Net/UnrealNetwork.h"

UASInventoryComponent::UASInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
}

void UASInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* Instance = GetWorld()->GetGameInstance(); Instance && GetNetMode() != NM_DedicatedServer)
	{
		LoadingFlow = Instance->GetSubsystem<UASLoadingFlowSubsystem>();
		if (LoadingFlow.IsValid()) LoadingFlow->OnLoadingStarted.AddUniqueDynamic(this, &ThisClass::HandleLoadingStarted);
	}
	PreloadItemAssets();
	if (HasInventoryAuthority() && !InitialItemsPreset.IsNull())
	{
		FString Error;
		if (!GrantInitialItems(Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[InitialItems] %s: %s"), *GetNameSafe(GetOwner()), *Error);
		}
	}
}

int32 UASInventoryComponent::AddItem(UASItemDefinition* ItemDefinition, const int32 RequestedQuantity)
{
	if (!HasInventoryAuthority() || !IsValid(ItemDefinition) || RequestedQuantity <= 0)
	{
		return 0;
	}
	if (ItemDefinition->ItemType == EASItemType::Quest && GetOwner()
		&& (GetOwner()->IsA<APawn>() || GetOwner()->IsA<AController>() || GetOwner()->IsA<APlayerState>())) return 0;

	RemoveEmptyAndInvalidInstances();

	int32 RemainingQuantity = RequestedQuantity;

	// Fill existing stacks before consuming additional inventory slots.
	for (UASItemInstance* ItemInstance : Items)
	{
		if (RemainingQuantity <= 0)
		{
			break;
		}

		if (IsValid(ItemInstance) && ItemInstance->InventorySlot >= 0 && ItemInstance->GetItemDefinition() == ItemDefinition)
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

		NewItemInstance->InventorySlot = FindFreeSlotIndex();
		Items.Add(NewItemInstance);
		RemainingQuantity -= NewItemInstance->GetQuantity();
	}

	const int32 AddedQuantity = RequestedQuantity - RemainingQuantity;
	if (AddedQuantity > 0)
	{
		NotifyInventoryChanged();
	}

	return AddedQuantity;
}

int32 UASInventoryComponent::RemoveItem(UASItemDefinition* ItemDefinition, const int32 RequestedQuantity)
{
	if (!HasInventoryAuthority() || !IsValid(ItemDefinition) || RequestedQuantity <= 0)
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

		if (ItemInstance->InventorySlot >= 0 && ItemInstance->GetItemDefinition() == ItemDefinition)
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
		NotifyInventoryChanged();
	}

	return RemovedQuantity;
}

int32 UASInventoryComponent::RemoveItemInstance(UASItemInstance* ItemInstance, const int32 RequestedQuantity)
{
	if (!HasInventoryAuthority() || !IsValid(ItemInstance) || ItemInstance->InventorySlot < 0 || RequestedQuantity <= 0)
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

	NotifyInventoryChanged();
	return RemovedQuantity;
}

int32 UASInventoryComponent::TransferItemTo(
	UASInventoryComponent* TargetInventory,
	UASItemInstance* ItemInstance,
	const int32 RequestedQuantity)
{
	if (!HasInventoryAuthority()
		|| !IsValid(TargetInventory)
		|| !TargetInventory->HasInventoryAuthority()
		|| TargetInventory == this
		|| !IsValid(ItemInstance)
		|| ItemInstance->InventorySlot < 0
		|| RequestedQuantity <= 0
		|| Items.IndexOfByKey(ItemInstance) == INDEX_NONE
		|| !ItemInstance->IsValidItemInstance())
	{
		return 0;
	}

	TargetInventory->RemoveEmptyAndInvalidInstances();
	if (ItemInstance->GetItemDefinition()->ItemType == EASItemType::Quest && TargetInventory->GetOwner()
		&& (TargetInventory->GetOwner()->IsA<APawn>() || TargetInventory->GetOwner()->IsA<AController>()
			|| TargetInventory->GetOwner()->IsA<APlayerState>())) return 0;

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

		if (IsValid(TargetItem) && TargetItem->InventorySlot >= 0 && TargetItem->CanStackWith(ItemInstance))
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

			TransferredInstance->InventorySlot = TargetInventory->FindFreeSlotIndex();
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

	NotifyInventoryChanged();
	TargetInventory->NotifyInventoryChanged();
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
	return GetMaxSlots() <= 0 || GetUsedSlotCount() < GetMaxSlots();
}

int32 UASInventoryComponent::GetUsedSlotCount() const
{
	int32 Count = 0;
	for (UASItemInstance* Item : Items)
		if (IsValid(Item) && Item->IsValidItemInstance() && Item->InventorySlot >= 0) ++Count;
	return Count;
}

int32 UASInventoryComponent::FindFreeSlotIndex() const
{
	const int32 Limit = GetMaxSlots() > 0 ? GetMaxSlots() : Items.Num() + 1;
	for (int32 Index = 0; Index < Limit; ++Index)
	{
		if (!GetItemAtSlot(Index)) return Index;
	}
	return INDEX_NONE;
}

UASItemInstance* UASInventoryComponent::GetItemAtSlot(const int32 SlotIndex) const
{
	if (SlotIndex != WeaponSlotIndex && SlotIndex != BagSlotIndex && !IsValidItemSlot(SlotIndex)) return nullptr;
	for (UASItemInstance* Item : Items)
	{
		if (IsValid(Item) && Item->IsValidItemInstance() && Item->InventorySlot == SlotIndex) return Item;
	}
	return nullptr;
}

bool UASInventoryComponent::SwapSlots(const int32 FromSlot, const int32 ToSlot,
	const FGuid ExpectedFromItem, const FGuid ExpectedToItem)
{
	if (!HasInventoryAuthority() || FromSlot == ToSlot
		|| (FromSlot != BagSlotIndex && !IsValidItemSlot(FromSlot))
		|| (ToSlot != BagSlotIndex && !IsValidItemSlot(ToSlot))) return false;
	UASItemInstance* From = GetItemAtSlot(FromSlot);
	UASItemInstance* To = GetItemAtSlot(ToSlot);
	if (!From || From->GetInstanceId() != ExpectedFromItem
		|| (To ? To->GetInstanceId() : FGuid()) != ExpectedToItem) return false;
	if (ToSlot == BagSlotIndex) return EquipBag(ExpectedFromItem, GetInventoryAvatar());
	if (FromSlot == BagSlotIndex)
	{
		if (To) return To->GetItemDefinition()->ItemType == EASItemType::Bag && EquipBag(ExpectedToItem, GetInventoryAvatar());
		return UnequipBag(GetInventoryAvatar(), ToSlot);
	}
	if (To && To->GetItemDefinition() == From->GetItemDefinition()
		&& To->GetItemDefinition()->MaxStackSize > 1)
	{
		// Match loot transfer rules. A full destination leaves both stacks in place.
		if (!To->CanStackWith(From)) return false;
		const int32 MergedQuantity = To->AddQuantity(From->GetQuantity());
		From->RemoveQuantity(MergedQuantity);
		if (!From->IsValidItemInstance()) Items.RemoveSingle(From);
		NotifyInventoryChanged();
		return true;
	}
	From->InventorySlot = ToSlot;
	if (To) To->InventorySlot = FromSlot;
	NotifyInventoryChanged();
	return true;
}

void UASInventoryComponent::RemoveEmptyAndInvalidInstances()
{
	Items.RemoveAll([](const TObjectPtr<UASItemInstance>& ItemInstance)
	{
		return !IsValid(ItemInstance) || !ItemInstance->IsValidItemInstance();
	});
}

bool UASInventoryComponent::HasInventoryAuthority() const
{
	return IsValid(GetOwner()) && GetOwner()->HasAuthority();
}

void UASInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UASInventoryComponent, Items);
}

bool UASInventoryComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWrote = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	// Supports existing player actors using the legacy path without changing their ownership code.
	for (UASItemInstance* Item : Items)
	{
		if (IsValid(Item))
		{
			bWrote |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
		}
	}
	return bWrote;
}

void UASInventoryComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	SyncReplicatedItems();
}

void UASInventoryComponent::SyncReplicatedItems()
{
	if (!HasInventoryAuthority()) return;
	for (UASItemInstance* Item : RegisteredItems)
	{
		if (!Items.Contains(Item)) RemoveReplicatedSubObject(Item);
	}
	for (UASItemInstance* Item : Items)
	{
		if (!RegisteredItems.Contains(Item)) AddReplicatedSubObject(Item);
	}
	RegisteredItems = Items;
}

void UASInventoryComponent::NotifyInventoryChanged()
{
	SyncReplicatedItems();
	PreloadItemAssets();
	GetOwner()->FlushNetDormancy();
	GetOwner()->ForceNetUpdate();
	OnInventoryChanged.Broadcast();
}

void UASInventoryComponent::OnRep_Items()
{
	PreloadItemAssets();
	OnInventoryChanged.Broadcast();
}

void UASInventoryComponent::HandleLoadingStarted()
{
	PreloadItemAssets();
}

void UASInventoryComponent::PreloadItemAssets()
{
	if (!GetWorld() || GetWorld()->bIsTearingDown) return;
	TArray<FSoftObjectPath> Paths;
	for (UASItemInstance* Item : Items)
	{
		if (!IsValid(Item) || !Item->IsValidItemInstance()) continue;
		for (const FSoftObjectPath& Path : ASItemAssetLoading::Gather(*Item->GetItemDefinition(), true)) Paths.AddUnique(Path);
	}
	InventoryAssetRequest.Request(Paths, FStreamableDelegate::CreateWeakLambda(this, [this]
	{
		if (GetWorld() && !GetWorld()->bIsTearingDown) OnInventoryChanged.Broadcast();
	}));
	InventoryAssetRequest.TrackLoadingScreen(this, NSLOCTEXT("ASItemLoading", "Inventory", "Preparing items and grenades..."));
}

UASItemInstance* UASInventoryComponent::FindItemById(const FGuid InstanceId) const
{
	for (UASItemInstance* Item : Items)
	{
		if (IsValid(Item) && Item->IsValidItemInstance() && Item->GetInstanceId() == InstanceId) return Item;
	}
	return nullptr;
}

bool UASInventoryComponent::TryConsumeItem(UASItemDefinition* Definition, const int32 Quantity)
{
	if (!HasInventoryAuthority() || Quantity <= 0 || !HasItem(Definition, Quantity)) return false;
	return RemoveItem(Definition, Quantity) == Quantity;
}

bool UASInventoryComponent::TryConsumeItemInstance(UASItemInstance* Item, const int32 Quantity)
{
	if (!HasInventoryAuthority() || !IsValid(Item) || Quantity <= 0
		|| !Items.Contains(Item) || !Item->IsValidItemInstance() || Item->GetQuantity() < Quantity) return false;
	return RemoveItemInstance(Item, Quantity) == Quantity;
}

void UASInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LoadingFlow.IsValid()) LoadingFlow->OnLoadingStarted.RemoveDynamic(this, &ThisClass::HandleLoadingStarted);
	LoadingFlow.Reset();
	CancelPendingItemAction();
	InventoryAssetRequest.Reset();
	// Equipment owns teardown of its actors and GAS grants.
	for (UASItemInstance* Item : RegisteredItems) RemoveReplicatedSubObject(Item);
	RegisteredItems.Reset();
	Super::EndPlay(EndPlayReason);
}
