#include "Items/Loot/ASLootDefinition.h"

#include "Items/Data/ASItemDefinition.h"
#include "Items/Loot/ASLootChest.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	bool IsLootItemValid(const UASItemDefinition* Item)
	{
		return IsValid(Item) && !Item->ItemId.IsNone() && Item->MaxStackSize > 0;
	}
}

bool FASLootPoolEntry::Validate(FString& OutError) const
{
	OutError.Reset();
	if (!IsLootItemValid(ItemDefinition) || !FMath::IsFinite(Weight) || Weight < 0.0f
		|| MinQuantity < 1 || MaxQuantity < MinQuantity)
	{
		OutError = TEXT("Candidates require a valid item, finite nonnegative weight, and 1 <= MinQuantity <= MaxQuantity.");
		return false;
	}
	return true;
}

bool FASLootChestPresetRow::ResolveRandomEntries(TArray<FASLootPoolEntry>& OutEntries, FString& OutError) const
{
	OutEntries.Reset();
	OutError.Reset();
	if (!RandomPool)
	{
		if (MaxRollCount == 0) return true;
		OutError = TEXT("Random rolls require an FASLootPoolEntry candidate DataTable.");
		return false;
	}
	if (RandomPool->GetRowStruct() != FASLootPoolEntry::StaticStruct())
	{
		OutError = TEXT("RandomPool must be a DataTable using FASLootPoolEntry rows.");
		return false;
	}

	TArray<FName> Rows = RandomPool->GetRowNames();
	Rows.Sort(FNameLexicalLess());
	TArray<FASLootPoolEntry> Resolved;
	TSet<FName> ItemIds;
	for (const FName RowName : Rows)
	{
		const FASLootPoolEntry* Entry = RandomPool->FindRow<FASLootPoolEntry>(RowName, TEXT("ResolveLootCandidates"), false);
		if (!Entry || !Entry->Validate(OutError))
		{
			OutError = FString::Printf(TEXT("Candidate row '%s': %s"), *RowName.ToString(),
				OutError.IsEmpty() ? TEXT("Unreadable row.") : *OutError);
			return false;
		}
		if (ItemIds.Contains(Entry->ItemDefinition->ItemId))
		{
			OutError = FString::Printf(TEXT("Duplicate candidate ItemId: %s."), *Entry->ItemDefinition->ItemId.ToString());
			return false;
		}
		ItemIds.Add(Entry->ItemDefinition->ItemId);
		Resolved.Add(*Entry);
	}
	OutEntries = MoveTemp(Resolved);
	return true;
}

bool FASLootChestPresetRow::Validate(FString& OutError) const
{
	OutError.Reset();
	if (!ChestClass || ChestClass->HasAnyClassFlags(CLASS_Abstract))
	{
		OutError = TEXT("Preset requires a concrete ChestClass.");
		return false;
	}
	if (MinRollCount < 0 || MaxRollCount < MinRollCount || MaxRollCount > 10000)
	{
		OutError = TEXT("Roll counts must satisfy 0 <= MinRollCount <= MaxRollCount <= 10000.");
		return false;
	}
	TMap<FName, const UASItemDefinition*> Ids;
	for (const FASItemGrant& Grant : FixedItems)
	{
		if (!IsLootItemValid(Grant.ItemDefinition) || Grant.Quantity <= 0)
		{
			OutError = TEXT("Fixed items require valid definitions and positive quantities.");
			return false;
		}
		const UASItemDefinition* const* Existing = Ids.Find(Grant.ItemDefinition->ItemId);
		if (Existing && *Existing != Grant.ItemDefinition)
		{
			OutError = TEXT("Different fixed item assets share an ItemId.");
			return false;
		}
		Ids.Add(Grant.ItemDefinition->ItemId, Grant.ItemDefinition);
	}
	for (const auto& Weight : CategoryWeights)
	{
		if ((Weight.Key != EASItemType::Weapon && Weight.Key != EASItemType::Ammo && Weight.Key != EASItemType::Consumable)
			|| !FMath::IsFinite(Weight.Value) || Weight.Value < 0.0f)
		{
			OutError = TEXT("Category weights require a valid category and finite nonnegative multiplier.");
			return false;
		}
	}
	for (const FASLootTagWeight& Weight : TagWeights)
	{
		if (!Weight.ItemTag.IsValid() || !FMath::IsFinite(Weight.Multiplier) || Weight.Multiplier < 0.0f)
		{
			OutError = TEXT("Tag weights require a valid tag and finite nonnegative multiplier.");
			return false;
		}
	}
	TArray<FASLootPoolEntry> Entries;
	if (!ResolveRandomEntries(Entries, OutError)) return false;
	for (const FASLootPoolEntry& Entry : Entries)
	{
		const UASItemDefinition* const* Existing = Ids.Find(Entry.ItemDefinition->ItemId);
		if (Existing && *Existing != Entry.ItemDefinition)
		{
			OutError = TEXT("Fixed items and candidate items disagree on an ItemId.");
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult FASLootPoolEntry::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}

EDataValidationResult FASLootChestPresetRow::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
#endif
