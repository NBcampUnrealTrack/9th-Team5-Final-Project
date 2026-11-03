#pragma once

#include "CoreMinimal.h"
#include "Items/Data/ASItemGrant.h"
#include "Items/Inventory/ASInventoryExchangeTypes.h"

class UASItemInstance;

struct FASInventoryPlannedStack
{
	UASItemDefinition* Definition = nullptr;
	int32 Quantity = 0;
	int32 Slot = INDEX_NONE;
};

/** Read-only plan shared by initial grants and crafting. No UObject creation or live mutation. */
struct FASInventoryMutationPlan
{
	TMap<UASItemInstance*, int32> Quantities;
	TArray<FASInventoryPlannedStack> NewStacks;

	static EASInventoryExchangeResult Build(const TArray<TObjectPtr<UASItemInstance>>& Items, int32 SlotLimit,
		const TArray<FASItemGrant>& Costs, const TArray<FASItemGrant>& Rewards, FASInventoryMutationPlan& OutPlan);
};
