#include "Items/Inventory/ASInventoryComponent.h"

#include "Items/Data/ASItemDefinition.h"
#include "Items/Data/ASStartingItemsRow.h"
#include "Items/Loot/ASLootValidation.h"
#include "Items/ASItemInstance.h"
#include "UObject/UObjectGlobals.h"
#include "AbilitySystemComponent.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/Use/ASItemUseComponent.h"
#include "Items/Equipment/ASItemEquipmentInterface.h"
#include "Items/Loot/ASDroppedItem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Core/Loading/ASLoadingFlowSubsystem.h"

#include "GAS/ASAbilityTags.h"

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
	// Player inventory is the ten visible slots. Loot containers keep their configured limit.
	if (Cast<APawn>(GetOwner()) || Cast<AController>(GetOwner()) || Cast<APlayerState>(GetOwner()))
	{
		MaxSlots = PlayerSlotCount;
	}

	if (HasInventoryAuthority() && !InitialItemsPreset.IsNull())
	{
		FString Error;
		if (!GrantInitialItems(Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[InitialItems] %s: %s"), *GetNameSafe(GetOwner()), *Error);
		}
	}
}

bool UASInventoryComponent::GrantInitialItems(FString& OutError)
{
	OutError.Reset();
	if (!HasInventoryAuthority() || GetNetMode() == NM_Client)
	{
		OutError = TEXT("Initial items require server authority.");
		return false;
	}
	if (bInitialItemsGranted) return true;
	if (!IsValid(InitialItemsPreset.DataTable) || InitialItemsPreset.RowName.IsNone()
		|| !InitialItemsPreset.DataTable->GetRowStruct()
		|| !InitialItemsPreset.DataTable->GetRowStruct()->IsChildOf(FASStartingItemsRow::StaticStruct()))
	{
		OutError = TEXT("Select a starting-items table and an ASStartingItemsRow row.");
		return false;
	}
	const FASStartingItemsRow* Row = InitialItemsPreset.DataTable->FindRow<FASStartingItemsRow>(
		InitialItemsPreset.RowName, TEXT("InitialItems"), false);
	if (!Row)
	{
		OutError = FString::Printf(TEXT("Starting-items row '%s' does not exist."), *InitialItemsPreset.RowName.ToString());
		return false;
	}
	if (!FASLootValidation::ValidateGrants(Row->Items, OutError)) return false;

	// Plan stack changes and new slots before touching the live inventory or broadcasting callbacks.
	TArray<TObjectPtr<UASItemInstance>> PendingItems;
	TMap<UASItemInstance*, int32> StackAdditions;
	TSet<int32> OccupiedSlots;
	for (UASItemInstance* Item : Items)
	{
		if (IsValid(Item) && Item->IsValidItemInstance() && Item->InventorySlot >= 0)
			OccupiedSlots.Add(Item->InventorySlot);
	}
	const bool bPlayerInventory = Cast<APawn>(GetOwner()) || Cast<AController>(GetOwner()) || Cast<APlayerState>(GetOwner());
	const int32 SlotLimit = bPlayerInventory ? PlayerSlotCount : MaxSlots;
	for (const FASItemGrant& Grant : Row->Items)
	{
		int32 Remaining = Grant.Quantity;
		for (UASItemInstance* Item : Items)
		{
			if (Remaining <= 0) break;
			if (!IsValid(Item) || !Item->IsValidItemInstance() || Item->InventorySlot < 0
				|| Item->GetItemDefinition() != Grant.ItemDefinition) continue;
			int32& Addition = StackAdditions.FindOrAdd(Item);
			const int32 Added = FMath::Min(Remaining, FMath::Max(0, Item->GetRemainingStackSpace() - Addition));
			Addition += Added;
			Remaining -= Added;
		}
		for (UASItemInstance* Item : PendingItems)
		{
			if (Remaining <= 0) break;
			if (Item->GetItemDefinition() == Grant.ItemDefinition)
				Remaining -= Item->AddQuantity(Remaining);
		}
		while (Remaining > 0)
		{
			int32 Slot = 0;
			while (OccupiedSlots.Contains(Slot)) ++Slot;
			if (SlotLimit > 0 && Slot >= SlotLimit)
			{
				OutError = FString::Printf(TEXT("Starting items exceed the inventory's %d backpack slots. No items were granted."), SlotLimit);
				return false;
			}
			UASItemInstance* Item = NewObject<UASItemInstance>(this);
			Item->Initialize(Grant.ItemDefinition, FMath::Min(Remaining, Grant.ItemDefinition->MaxStackSize));
			Item->InventorySlot = Slot;
			PendingItems.Add(Item);
			OccupiedSlots.Add(Slot);
			Remaining -= Item->GetQuantity();
		}
	}

	bool bChanged = !PendingItems.IsEmpty();
	for (const auto& Addition : StackAdditions)
	{
		Addition.Key->AddQuantity(Addition.Value);
		bChanged |= Addition.Value > 0;
	}
	Items.Append(PendingItems);
	bInitialItemsGranted = true; // Set before notification so reentrant calls cannot grant twice.
	if (bChanged) NotifyInventoryChanged();
	return true;
}

int32 UASInventoryComponent::AddItem(UASItemDefinition* ItemDefinition, const int32 RequestedQuantity)
{
	if (!HasInventoryAuthority() || !IsValid(ItemDefinition) || RequestedQuantity <= 0)
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
	return MaxSlots <= 0 || GetUsedSlotCount() < MaxSlots;
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
	const int32 Limit = MaxSlots > 0 ? MaxSlots : Items.Num() + 1;
	for (int32 Index = 0; Index < Limit; ++Index)
	{
		if (!GetItemAtSlot(Index)) return Index;
	}
	return INDEX_NONE;
}

UASItemInstance* UASInventoryComponent::GetItemAtSlot(const int32 SlotIndex) const
{
	if ((SlotIndex < 0 && SlotIndex != WeaponSlotIndex) || (MaxSlots > 0 && SlotIndex >= MaxSlots)) return nullptr;
	for (UASItemInstance* Item : Items)
	{
		if (IsValid(Item) && Item->IsValidItemInstance() && Item->InventorySlot == SlotIndex) return Item;
	}
	return nullptr;
}

bool UASInventoryComponent::SwapSlots(const int32 FromSlot, const int32 ToSlot,
	const FGuid ExpectedFromItem, const FGuid ExpectedToItem)
{
	if (!HasInventoryAuthority() || FromSlot == ToSlot || FromSlot < 0 || ToSlot < 0
		|| FromSlot >= PlayerSlotCount || ToSlot >= PlayerSlotCount) return false;
	UASItemInstance* From = GetItemAtSlot(FromSlot);
	UASItemInstance* To = GetItemAtSlot(ToSlot);
	if (!From || From->GetInstanceId() != ExpectedFromItem
		|| (To ? To->GetInstanceId() : FGuid()) != ExpectedToItem) return false;
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

AASDroppedItem* UASInventoryComponent::DropSlot(const int32 SlotIndex, const FGuid ExpectedItem, APawn* Avatar)
{
	return DropSlotInternal(SlotIndex, ExpectedItem, Avatar, false);
}

AASDroppedItem* UASInventoryComponent::DropOneFromSlot(const int32 SlotIndex, const FGuid ExpectedItem, APawn* Avatar)
{
	return DropSlotInternal(SlotIndex, ExpectedItem, Avatar, true);
}

AASDroppedItem* UASInventoryComponent::DropSlotInternal(const int32 SlotIndex, const FGuid ExpectedItem,
	APawn* Avatar, const bool bSingleItem)
{
	if (!HasInventoryAuthority() || !IsValid(Avatar) || Avatar != GetInventoryAvatar()
		|| SlotIndex < 0 || SlotIndex >= PlayerSlotCount || !GetWorld()) return nullptr;
	UASItemInstance* Item = GetItemAtSlot(SlotIndex);
	UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(this, Avatar);
	if (!Item || Item->GetInstanceId() != ExpectedItem || !ASC
		|| ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)) return nullptr;

	FVector Location = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 100.0f;
	FHitResult Ground;
	FCollisionQueryParams Query;
	Query.AddIgnoredActor(Avatar);
	if (GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0, 0, 100),
		Location - FVector(0, 0, 300), ECC_Visibility, Query))
		Location = Ground.ImpactPoint + FVector(0, 0, 30);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AASDroppedItem* Dropped = GetWorld()->SpawnActor<AASDroppedItem>(Location, FRotator::ZeroRotator, Spawn);
	if (!Dropped) return nullptr;
	if (GetEquippedItem() == Item)
	{
		UnequipItem(); // Equipment implementation owns actor/GAS cleanup.
		if (GetEquippedItem() == Item)
		{
			Dropped->Destroy();
			return nullptr;
		}
	}
	// Unequip callbacks may change ownership or quantity. Revalidate before transferring.
	if (!IsValid(Avatar) || Avatar != GetInventoryAvatar() || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
		|| GetItemAtSlot(SlotIndex) != Item || !Item->IsValidItemInstance()
		|| TransferItemTo(Dropped->GetLootInventory(), Item, bSingleItem ? 1 : Item->GetQuantity()) <= 0)
	{
		Dropped->Destroy();
		return nullptr;
	}
	return Dropped;
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

void UASInventoryComponent::CancelPendingItemAction()
{
	++PendingActionSerial;
	PendingActionItem.Invalidate();
	PendingActionAvatar.Reset();
	ActionAssetRequest.Reset();
}

bool UASInventoryComponent::PrepareItemAction(const FGuid InstanceId, APawn* Avatar,
	const bool bEquipOnly, bool& bOutQueued)
{
	bOutQueued = false;
	UASItemInstance* Item = FindItemById(InstanceId);
	UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(this, Avatar);
	if (!HasInventoryAuthority() || !Item || !ASC || Avatar != GetInventoryAvatar()
		|| ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)) return false;
	const UASItemDefinition* Definition = Item->GetItemDefinition();
	if (Definition->ItemType != EASItemType::Weapon && Definition->ItemType != EASItemType::Consumable) return false;
	if (bEquipOnly && Definition->ItemType != EASItemType::Weapon) return false;
	if (Definition->ItemType == EASItemType::Consumable
		&& (Definition->UseQuantity <= 0 || Item->GetQuantity() < Definition->UseQuantity)) return false;
	const TArray<FSoftObjectPath> Paths = ASItemAssetLoading::Gather(*Definition, false);
	if (ASItemAssetLoading::AreLoaded(Paths))
	{
		CancelPendingItemAction();
		return true;
	}
	if (PendingActionItem == InstanceId && PendingActionAvatar.Get() == Avatar && bPendingEquipOnly == bEquipOnly)
	{
		bOutQueued = true;
		return true;
	}
	CancelPendingItemAction();
	PendingActionItem = InstanceId;
	PendingActionAvatar = Avatar;
	bPendingEquipOnly = bEquipOnly;
	const uint64 Serial = PendingActionSerial;
	const TWeakObjectPtr<UASItemDefinition> ExpectedDefinition(Item->GetItemDefinition());
	bOutQueued = ActionAssetRequest.Request(Paths, FStreamableDelegate::CreateWeakLambda(this,
		[this, Serial, InstanceId, ExpectedDefinition, Paths, WeakAvatar = TWeakObjectPtr<APawn>(Avatar), bEquipOnly]
		{
			if (Serial != PendingActionSerial) return;
			UASItemInstance* Current = FindItemById(InstanceId);
			APawn* CurrentAvatar = WeakAvatar.Get();
			if (!Current || !CurrentAvatar || !GetWorld() || GetWorld()->bIsTearingDown
				|| Current->GetItemDefinition() != ExpectedDefinition.Get()
				|| GetInventoryAvatar() != CurrentAvatar || !ASItemAssetLoading::AreLoaded(Paths))
			{
				CancelPendingItemAction();
				return;
			}
			// Revalidate authority, avatar, death and quantity again through the public operation.
			if (bEquipOnly) EquipItem(InstanceId, CurrentAvatar);
			else UseItem(InstanceId, CurrentAvatar);
			if (Serial == PendingActionSerial) CancelPendingItemAction();
		}));
	if (!bOutQueued) CancelPendingItemAction();
	return bOutQueued;
}

UASItemInstance* UASInventoryComponent::FindItemById(const FGuid InstanceId) const
{
	for (UASItemInstance* Item : Items)
	{
		if (IsValid(Item) && Item->IsValidItemInstance() && Item->GetInstanceId() == InstanceId) return Item;
	}
	return nullptr;
}

bool UASInventoryComponent::UseItem(const FGuid InstanceId, APawn* Avatar)
{
	if (!HasInventoryAuthority()) return false;
	bool bQueued = false;
	if (!PrepareItemAction(InstanceId, Avatar, false, bQueued)) return false;
	if (bQueued) return true;
	if (!IsValid(ItemUseComponent))
	{
		ItemUseComponent = GetOwner()->FindComponentByClass<UASItemUseComponent>();
		if (!ItemUseComponent)
		{
			ItemUseComponent = NewObject<UASItemUseComponent>(GetOwner());
			ItemUseComponent->RegisterComponent();
		}
	}
	return ItemUseComponent->UseItem(this, InstanceId, Avatar);
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

bool UASInventoryComponent::EquipItem(const FGuid InstanceId, APawn* Avatar)
{
	if (!HasInventoryAuthority() || bChangingEquipment) return false;
	UASItemInstance* Item = FindItemById(InstanceId);
	UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(this, Avatar);
	if (!Item || !ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
		|| Item->GetItemDefinition()->ItemType != EASItemType::Weapon) return false;
	if (Avatar != GetInventoryAvatar()) return false;
	UActorComponent* Provider = GetEquipmentProvider();
	if (!Provider) return false;
	UASItemInstance* Previous = GetEquippedItem();
	if (!Previous) Previous = GetItemAtSlot(WeaponSlotIndex);
	const int32 SourceSlot = Item->InventorySlot;
	if (SourceSlot < 0 && (SourceSlot != WeaponSlotIndex || Previous != Item)) return false;
	bool bQueued = false;
	if (!PrepareItemAction(InstanceId, Avatar, true, bQueued)) return false;
	if (bQueued) return true;
	TGuardValue<bool> Guard(bChangingEquipment, true);
	if (!IASItemEquipmentInterface::Execute_EquipInventoryItem(Provider, this, Item, Avatar)
		|| GetEquippedItem() != Item) return false;
	// Keep the same owned/replicated instance; only its UI position changes.
	if (Previous && Previous != Item) Previous->InventorySlot = SourceSlot;
	Item->InventorySlot = WeaponSlotIndex;
	NotifyInventoryChanged();
	return true;
}

void UASInventoryComponent::UnequipItem()
{
	if (!HasInventoryAuthority() || bChangingEquipment) return;
	CancelPendingItemAction();
	APawn* Avatar = GetInventoryAvatar();
	UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(this, Avatar);
	if (!ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)) return;
	UASItemInstance* Item = GetEquippedItem();
	if (!Item) return;
	const int32 ReturnSlot = Item->InventorySlot >= 0 ? Item->InventorySlot : FindFreeSlotIndex();
	if (ReturnSlot == INDEX_NONE) return;
	if (UActorComponent* Provider = GetEquipmentProvider())
	{
		TGuardValue<bool> Guard(bChangingEquipment, true);
		if (!IASItemEquipmentInterface::Execute_UnequipInventoryItem(Provider, this, Avatar) || GetEquippedItem()) return;
		Item->InventorySlot = ReturnSlot;
		NotifyInventoryChanged();
	}
}

UASItemInstance* UASInventoryComponent::GetEquippedItem() const
{
	UActorComponent* Provider = GetEquipmentProvider();
	UASItemInstance* Item = Provider ? IASItemEquipmentInterface::Execute_GetEquippedInventoryItem(Provider, this) : nullptr;
	return IsValid(Item) && Item->IsValidItemInstance() && Items.Contains(Item)
		&& Item->GetItemDefinition()->ItemType == EASItemType::Weapon ? Item : nullptr;
}

AActor* UASInventoryComponent::GetEquippedActor() const
{
	if (!GetEquippedItem()) return nullptr;
	UActorComponent* Provider = GetEquipmentProvider();
	AActor* Actor = Provider ? IASItemEquipmentInterface::Execute_GetEquippedInventoryActor(Provider, this) : nullptr;
	return IsValid(Actor) ? Actor : nullptr;
}

APawn* UASInventoryComponent::GetInventoryAvatar() const
{
	if (APawn* Pawn = Cast<APawn>(GetOwner())) return Pawn;
	if (AController* Controller = Cast<AController>(GetOwner())) return Controller->GetPawn();
	if (APlayerState* State = Cast<APlayerState>(GetOwner())) return State->GetPawn();
	return nullptr;
}

UActorComponent* UASInventoryComponent::GetEquipmentProvider() const
{
	// Prototype contract: the equipment component lives on the current player pawn.
	APawn* Avatar = GetInventoryAvatar();
	if (!IsValid(Avatar)) return nullptr;
	TArray<UActorComponent*> Components;
	Avatar->GetComponents(Components);
	UActorComponent* Result = nullptr;
	for (UActorComponent* Component : Components)
	{
		if (!IsValid(Component) || !Component->IsRegistered()
			|| !Component->Implements<UASItemEquipmentInterface>()) continue;
		if (Result) return nullptr;
		Result = Component;
	}
	return Result;
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
