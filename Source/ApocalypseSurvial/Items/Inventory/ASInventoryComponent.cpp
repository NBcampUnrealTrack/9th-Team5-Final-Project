#include "Items/Inventory/ASInventoryComponent.h"

#include "Items/Data/ASItemDefinition.h"
#include "Items/ASItemInstance.h"
#include "UObject/UObjectGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Items/GAS/ASItemUseAbility.h"
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
	DOREPLIFETIME(UASInventoryComponent, EquippedItem);
	DOREPLIFETIME(UASInventoryComponent, EquippedActor);
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
	if (IsValid(EquippedItem) && (!Items.Contains(EquippedItem) || !EquippedItem->IsValidItemInstance()))
	{
		ClearEquipment();
	}
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

UAbilitySystemComponent* UASInventoryComponent::ResolveAbilitySystem(APawn* Avatar) const
{
	if (!IsValid(Avatar) || (GetOwner() != Avatar && GetOwner() != Avatar->GetPlayerState())) return nullptr;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Avatar, true);
	return IsValid(ASC) && ASC->GetAvatarActor() == Avatar ? ASC : nullptr;
}

bool UASInventoryComponent::UseItem(const FGuid InstanceId, APawn* Avatar)
{
	if (!HasInventoryAuthority()) return false;
	UASItemInstance* Item = FindItemById(InstanceId);
	UAbilitySystemComponent* ASC = ResolveAbilitySystem(Avatar);
	if (!IsValid(Item) || !ASC || Item->GetItemDefinition()->ItemType != EASItemType::Consumable) return false;
	UClass* AbilityClass = Item->GetItemDefinition()->UseAbility.IsNull()
		? UASItemUseAbility::StaticClass() : Item->GetItemDefinition()->UseAbility.LoadSynchronous();
	// The item-use base class charges one unit on GAS CommitAbility, never merely on activation.
	if (!AbilityClass || !AbilityClass->IsChildOf(UASItemUseAbility::StaticClass())) return false;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.SourceObject.Get() == Item && Spec.IsActive()) return false;
	}
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, Item));
	const bool bActivated = ASC->TryActivateAbility(Handle);
	if (bActivated) ASC->SetRemoveAbilityOnEnd(Handle);
	else ASC->ClearAbility(Handle);
	return bActivated;
}

bool UASInventoryComponent::EquipItem(const FGuid InstanceId, APawn* Avatar)
{
	if (!HasInventoryAuthority()) return false;
	UASItemInstance* Item = FindItemById(InstanceId);
	UAbilitySystemComponent* ASC = ResolveAbilitySystem(Avatar);
	if (!IsValid(Item) || !ASC || Item->GetItemDefinition()->ItemType != EASItemType::Weapon) return false;
	if (EquippedItem == Item && EquippedAbilitySystem == ASC) return true;

	UASItemDefinition* Definition = Item->GetItemDefinition();
	TArray<UClass*> AbilityClasses;
	for (const TSoftClassPtr<UGameplayAbility>& SoftClass : Definition->GrantedAbilities)
	{
		UClass* AbilityClass = SoftClass.LoadSynchronous();
		if (!AbilityClass) return false;
		AbilityClasses.Add(AbilityClass);
	}
	UClass* ActorClass = Definition->EquippedActorClass.IsNull() ? nullptr : Definition->EquippedActorClass.LoadSynchronous();
	if (!Definition->EquippedActorClass.IsNull() && !ActorClass) return false;
	AActor* NewActor = nullptr;
	if (ActorClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = Avatar;
		Params.Instigator = Avatar;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		NewActor = GetWorld()->SpawnActor<AActor>(ActorClass, Avatar->GetActorTransform(), Params);
		if (!NewActor) return false;
		NewActor->SetReplicates(true);
		USceneComponent* AttachComponent = Avatar->GetRootComponent();
		if (ACharacter* Character = Cast<ACharacter>(Avatar)) AttachComponent = Character->GetMesh();
		if (!AttachComponent || !NewActor->AttachToComponent(AttachComponent,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale, Definition->EquippedSocketName))
		{
			NewActor->Destroy();
			return false;
		}
	}
	ClearEquipment();
	EquippedItem = Item;
	EquippedActor = NewActor;
	EquippedAbilitySystem = ASC;
	for (UClass* AbilityClass : AbilityClasses)
	{
		EquippedAbilityHandles.Add(ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, Item)));
	}
	NotifyInventoryChanged();
	return true;
}

void UASInventoryComponent::UnequipItem()
{
	if (!HasInventoryAuthority()) return;
	ClearEquipment();
	NotifyInventoryChanged();
}

void UASInventoryComponent::ClearEquipment()
{
	if (UAbilitySystemComponent* ASC = EquippedAbilitySystem.Get())
	{
		for (const FGameplayAbilitySpecHandle Handle : EquippedAbilityHandles)
		{
			ASC->CancelAbilityHandle(Handle);
			ASC->ClearAbility(Handle);
		}
	}
	EquippedAbilityHandles.Reset();
	EquippedAbilitySystem.Reset();
	if (IsValid(EquippedActor)) EquippedActor->Destroy();
	EquippedActor = nullptr;
	EquippedItem = nullptr;
}

void UASInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasInventoryAuthority()) ClearEquipment();
	for (UASItemInstance* Item : RegisteredItems) RemoveReplicatedSubObject(Item);
	RegisteredItems.Reset();
	Super::EndPlay(EndPlayReason);
}
