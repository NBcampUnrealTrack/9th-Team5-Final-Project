#include "Items/Inventory/ASInventoryComponent.h"

#include "Items/Data/ASItemDefinition.h"
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

#include "GAS/ASAbilityTags.h"

#include "Net/UnrealNetwork.h"

UASInventoryComponent::UASInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
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
		NotifyInventoryChanged();
	}

	return RemovedQuantity;
}

int32 UASInventoryComponent::RemoveItemInstance(UASItemInstance* ItemInstance, const int32 RequestedQuantity)
{
	if (!HasInventoryAuthority() || !IsValid(ItemInstance) || RequestedQuantity <= 0)
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
	return MaxSlots <= 0 || Items.Num() < MaxSlots;
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
	GetOwner()->FlushNetDormancy();
	GetOwner()->ForceNetUpdate();
	OnInventoryChanged.Broadcast();
}

void UASInventoryComponent::OnRep_Items()
{
	OnInventoryChanged.Broadcast();
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

bool UASInventoryComponent::CanCommitWeaponAttack(UASItemInstance* Weapon, APawn* Avatar) const
{
	UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(this, Avatar);
	if (!HasInventoryAuthority() || bCommittingWeaponAttack || !ASC
		|| ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
		|| !IsValid(Weapon) || !Weapon->IsValidItemInstance() || !Items.Contains(Weapon)
		|| Avatar != GetInventoryAvatar() || GetEquippedItem() != Weapon
		|| !GetWorld() || GetWorld()->GetTimeSeconds() < NextWeaponAttackTime) return false;
	const UASItemDefinition* Definition = Weapon->GetItemDefinition();
	if (Definition->ItemType != EASItemType::Weapon) return false;
	if (!Definition->bUsesAmmo) return true;
	return Definition->WeaponType == EASWeaponType::Ranged
		&& IsValid(Definition->AmmoDefinition) && Definition->AmmoDefinition->ItemType == EASItemType::Ammo
		&& Definition->AmmoPerShot > 0 && HasItem(Definition->AmmoDefinition, Definition->AmmoPerShot);
}

bool UASInventoryComponent::TryCommitWeaponAttack(UASItemInstance* Weapon, APawn* Avatar)
{
	if (!CanCommitWeaponAttack(Weapon, Avatar)) return false;
	TGuardValue<bool> Guard(bCommittingWeaponAttack, true);
	const UASItemDefinition* Definition = Weapon->GetItemDefinition();
	const double PreviousTime = NextWeaponAttackTime;
	// Establish the interval before inventory callbacks can request another attack.
	NextWeaponAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.01f, Definition->AttackInterval);
	if (Definition->bUsesAmmo && !TryConsumeItem(Definition->AmmoDefinition, Definition->AmmoPerShot))
	{
		NextWeaponAttackTime = PreviousTime;
		return false;
	}
	return true;
}

bool UASInventoryComponent::EquipItem(const FGuid InstanceId, APawn* Avatar)
{
	if (!HasInventoryAuthority()) return false;
	UASItemInstance* Item = FindItemById(InstanceId);
	UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(this, Avatar);
	if (!Item || !ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
		|| Item->GetItemDefinition()->ItemType != EASItemType::Weapon) return false;
	if (Avatar != GetInventoryAvatar()) return false;
	UActorComponent* Provider = GetEquipmentProvider();
	return Provider && IASItemEquipmentInterface::Execute_EquipInventoryItem(Provider, this, Item, Avatar);
}

void UASInventoryComponent::UnequipItem()
{
	if (!HasInventoryAuthority()) return;
	APawn* Avatar = GetInventoryAvatar();
	if (UActorComponent* Provider = GetEquipmentProvider())
	{
		IASItemEquipmentInterface::Execute_UnequipInventoryItem(Provider, this, Avatar);
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
	// Equipment owns teardown of its actors and GAS grants.
	for (UASItemInstance* Item : RegisteredItems) RemoveReplicatedSubObject(Item);
	RegisteredItems.Reset();
	Super::EndPlay(EndPlayReason);
}
