#include "Items/Inventory/ASInventoryComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Data/ASStartingItemsRow.h"
#include "Items/Inventory/ASInventoryMutationPlan.h"
#include "Items/Loot/ASLootValidation.h"

int32 UASInventoryComponent::GetBackpackSlotLimit() const
{
	return GetMaxSlots();
}

bool UASInventoryComponent::ApplyMutationPlan(const FASInventoryMutationPlan& Plan)
{
	// Build all new instances before changing live state. No delegates run until the caller publishes.
	TArray<TObjectPtr<UASItemInstance>> PendingItems;
	for (const FASInventoryPlannedStack& Stack : Plan.NewStacks)
	{
		UASItemInstance* Item = NewObject<UASItemInstance>(this);
		Item->Initialize(Stack.Definition, Stack.Quantity);
		Item->InventorySlot = Stack.Slot;
		PendingItems.Add(Item);
	}
	bool bChanged = !PendingItems.IsEmpty();
	for (const auto& Entry : Plan.Quantities)
	{
		bChanged |= Entry.Key->Quantity != Entry.Value;
		Entry.Key->Quantity = Entry.Value;
	}
	const int32 PreviousCount = Items.Num();
	RemoveEmptyAndInvalidInstances();
	bChanged |= Items.Num() != PreviousCount;
	Items.Append(PendingItems);
	return bChanged;
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
	FASInventoryMutationPlan Plan;
	if (FASInventoryMutationPlan::Build(Items, GetBackpackSlotLimit(), {}, Row->Items, Plan) != EASInventoryExchangeResult::Success)
	{
		OutError = FString::Printf(TEXT("Starting items exceed the inventory's %d backpack slots. No items were granted."), GetBackpackSlotLimit());
		return false;
	}
	const bool bChanged = ApplyMutationPlan(Plan);
	bInitialItemsGranted = true; // Mark before publishing to prevent reentrant grants.
	if (bChanged) NotifyInventoryChanged();
	return true;
}

EASInventoryExchangeResult UASInventoryComponent::CanExchangeItems(const TArray<FASItemGrant>& Costs,
	const TArray<FASItemGrant>& Rewards) const
{
	if (Costs.IsEmpty() || Rewards.IsEmpty()) return EASInventoryExchangeResult::InvalidRequest;
	FASInventoryMutationPlan Plan;
	return FASInventoryMutationPlan::Build(Items, GetBackpackSlotLimit(), Costs, Rewards, Plan);
}

EASInventoryExchangeResult UASInventoryComponent::TryExchangeItems(const TArray<FASItemGrant>& Costs,
	const TArray<FASItemGrant>& Rewards)
{
	if (!HasInventoryAuthority() || GetNetMode() == NM_Client) return EASInventoryExchangeResult::NoAuthority;
	if (Costs.IsEmpty() || Rewards.IsEmpty()) return EASInventoryExchangeResult::InvalidRequest;
	FASInventoryMutationPlan Plan;
	const EASInventoryExchangeResult Result = FASInventoryMutationPlan::Build(Items, GetBackpackSlotLimit(), Costs, Rewards, Plan);
	if (Result != EASInventoryExchangeResult::Success) return Result;
	ApplyMutationPlan(Plan);
	NotifyInventoryChanged();
	return EASInventoryExchangeResult::Success;
}
