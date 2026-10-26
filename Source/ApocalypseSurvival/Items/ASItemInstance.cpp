#include "Items/ASItemInstance.h"

#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Net/UnrealNetwork.h"

void UASItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UASItemInstance, InstanceId);
	DOREPLIFETIME(UASItemInstance, ItemDefinition);
	DOREPLIFETIME(UASItemInstance, Quantity);
	DOREPLIFETIME(UASItemInstance, InventorySlot);
}

UWorld* UASItemInstance::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UASItemInstance::PostNetReceive()
{
	Super::PostNetReceive();
	if (UASInventoryComponent* Inventory = GetTypedOuter<UASInventoryComponent>())
	{
		Inventory->PreloadItemAssets();
		Inventory->OnInventoryChanged.Broadcast();
	}
}

void UASItemInstance::Initialize(UASItemDefinition* InItemDefinition, const int32 InQuantity)
{
	ItemDefinition = InItemDefinition;
	InstanceId = FGuid::NewGuid();

	if (!IsValid(ItemDefinition))
	{
		Quantity = 0;
		return;
	}

	const int32 MaxStackSize = FMath::Max(1, ItemDefinition->MaxStackSize);
	Quantity = FMath::Clamp(InQuantity, 1, MaxStackSize);
}

bool UASItemInstance::IsValidItemInstance() const
{
	return IsValid(ItemDefinition) && Quantity > 0 && InstanceId.IsValid();
}

EASLootRarity UASItemInstance::GetRarity() const
{
	return IsValid(ItemDefinition) ? ItemDefinition->Rarity : EASLootRarity::Uncommon;
}

int32 UASItemInstance::GetRemainingStackSpace() const
{
	if (!IsValid(ItemDefinition))
	{
		return 0;
	}

	const int32 MaxStackSize = FMath::Max(1, ItemDefinition->MaxStackSize);
	return FMath::Max(0, MaxStackSize - Quantity);
}

bool UASItemInstance::CanStackWith(const UASItemInstance* Other) const
{
	return IsValid(Other)
		&& IsValidItemInstance()
		&& Other->IsValidItemInstance()
		&& ItemDefinition == Other->ItemDefinition
		&& GetRemainingStackSpace() > 0;
}

int32 UASItemInstance::AddQuantity(const int32 RequestedQuantity)
{
	if (RequestedQuantity <= 0 || !IsValidItemInstance())
	{
		return 0;
	}

	const int32 AddedQuantity = FMath::Min(RequestedQuantity, GetRemainingStackSpace());
	Quantity += AddedQuantity;
	return AddedQuantity;
}

int32 UASItemInstance::RemoveQuantity(const int32 RequestedQuantity)
{
	if (RequestedQuantity <= 0 || !IsValidItemInstance())
	{
		return 0;
	}

	const int32 RemovedQuantity = FMath::Min(RequestedQuantity, Quantity);
	Quantity -= RemovedQuantity;
	return RemovedQuantity;
}
