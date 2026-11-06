#include "Items/Inventory/ASInventoryComponent.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ASAbilityTags.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Equipment/ASItemEquipmentInterface.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/Loot/ASDroppedItem.h"
#include "Items/Use/ASItemUseComponent.h"

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
		|| !IsValidItemSlot(SlotIndex) || !GetWorld()) return nullptr;
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

bool UASInventoryComponent::UseItem(const FGuid InstanceId, APawn* Avatar)
{
	if (!HasInventoryAuthority()) return false;
	if (UASItemInstance* Item = FindItemById(InstanceId); Item && Item->GetItemDefinition()->ItemType == EASItemType::Bag)
		return Item == GetEquippedBag() ? UnequipBag(Avatar) : EquipBag(InstanceId, Avatar);
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
