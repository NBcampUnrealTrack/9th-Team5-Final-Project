#include "Items/Inventory/ASInventoryMutationPlan.h"

#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"

EASInventoryExchangeResult FASInventoryMutationPlan::Build(const TArray<TObjectPtr<UASItemInstance>>& Items,
	int32 SlotLimit, const TArray<FASItemGrant>& Costs, const TArray<FASItemGrant>& Rewards,
	FASInventoryMutationPlan& OutPlan)
{
	OutPlan = FASInventoryMutationPlan();
	FASInventoryMutationPlan Plan;
	TMap<UASItemDefinition*, int64> Required;
	for (const FASItemGrant& Cost : Costs)
	{
		if (!IsValid(Cost.ItemDefinition) || Cost.Quantity <= 0 || Cost.ItemDefinition->MaxStackSize < 1)
			return EASInventoryExchangeResult::InvalidRequest;
		int64& Total = Required.FindOrAdd(Cost.ItemDefinition);
		Total += Cost.Quantity;
		if (Total > MAX_int32) return EASInventoryExchangeResult::InvalidRequest;
	}
	for (const FASItemGrant& Reward : Rewards)
		if (!IsValid(Reward.ItemDefinition) || Reward.Quantity <= 0 || Reward.ItemDefinition->MaxStackSize < 1)
			return EASInventoryExchangeResult::InvalidRequest;

	TSet<int32> Occupied;
	for (UASItemInstance* Item : Items)
	{
		if (!IsValid(Item) || !Item->IsValidItemInstance() || Item->GetInventorySlot() < 0) continue;
		int32 Quantity = Item->GetQuantity();
		if (int64* Remaining = Required.Find(Item->GetItemDefinition()))
		{
			const int32 Consumed = static_cast<int32>(FMath::Min<int64>(Quantity, *Remaining));
			Quantity -= Consumed;
			*Remaining -= Consumed;
		}
		Plan.Quantities.Add(Item, Quantity);
		if (Quantity > 0) Occupied.Add(Item->GetInventorySlot());
	}
	for (const auto& Entry : Required)
		if (Entry.Value > 0) return EASInventoryExchangeResult::MissingMaterials;

	for (const FASItemGrant& Reward : Rewards)
	{
		int32 Remaining = Reward.Quantity;
		for (UASItemInstance* Item : Items)
		{
			int32* Quantity = Plan.Quantities.Find(Item);
			if (!Quantity || *Quantity <= 0 || Item->GetItemDefinition() != Reward.ItemDefinition) continue;
			const int32 Added = FMath::Min(Remaining, FMath::Max(0, Reward.ItemDefinition->MaxStackSize - *Quantity));
			*Quantity += Added;
			Remaining -= Added;
		}
		for (FASInventoryPlannedStack& Stack : Plan.NewStacks)
		{
			if (Stack.Definition != Reward.ItemDefinition) continue;
			const int32 Added = FMath::Min(Remaining, Reward.ItemDefinition->MaxStackSize - Stack.Quantity);
			Stack.Quantity += Added;
			Remaining -= Added;
		}
		while (Remaining > 0)
		{
			int32 Slot = 0;
			while (Occupied.Contains(Slot)) ++Slot;
			if (SlotLimit > 0 && Slot >= SlotLimit) return EASInventoryExchangeResult::InventoryFull;
			FASInventoryPlannedStack& Stack = Plan.NewStacks.AddDefaulted_GetRef();
			Stack.Definition = Reward.ItemDefinition;
			Stack.Quantity = FMath::Min(Remaining, Reward.ItemDefinition->MaxStackSize);
			Stack.Slot = Slot;
			Occupied.Add(Slot);
			Remaining -= Stack.Quantity;
		}
	}
	OutPlan = MoveTemp(Plan);
	return EASInventoryExchangeResult::Success;
}
