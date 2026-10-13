#include "Items/Loot/ASLootValidation.h"
#include "Items/Data/ASItemDefinition.h"

bool FASLootValidation::ValidateItem(const UASItemDefinition* Item, FString& OutError)
{
	OutError.Reset();
	if (!IsValid(Item) || Item->ItemId.IsNone() || Item->MaxStackSize <= 0)
	{
		OutError = TEXT("Loot requires a valid item definition, nonempty ItemId, and positive MaxStackSize.");
		return false;
	}
	return true;
}

bool FASLootValidation::ValidateGrants(const TArray<FASItemGrant>& Items, FString& OutError)
{
	OutError.Reset();
	TMap<FName, const UASItemDefinition*> Ids;
	TMap<const UASItemDefinition*, int64> Totals;
	for (const FASItemGrant& Grant : Items)
	{
		if (!ValidateItem(Grant.ItemDefinition, OutError)) return false;
		if (Grant.Quantity <= 0) { OutError = TEXT("Loot quantities must be positive."); return false; }
		const UASItemDefinition* Item = Grant.ItemDefinition;
		const UASItemDefinition* const* Existing = Ids.Find(Item->ItemId);
		if (Existing && *Existing != Item)
		{
			OutError = FString::Printf(TEXT("Different item assets share ItemId '%s'."), *Item->ItemId.ToString());
			return false;
		}
		Ids.Add(Item->ItemId, Item);
		int64& Total = Totals.FindOrAdd(Item);
		Total += Grant.Quantity;
		if (Total > MAX_int32)
		{
			OutError = FString::Printf(TEXT("Loot quantity overflow for '%s'."), *Item->ItemId.ToString());
			return false;
		}
	}
	return true;
}
