#include "Items/Loot/ASMonsterDropDefinition.h"

#include "Items/Data/ASItemDefinition.h"
#include "Items/Loot/ASLootValidation.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FASMonsterDropGroupRow::ResolveRandomEntries(TArray<FASLootPoolEntry>& OutEntries, FString& OutError) const
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
	TSet<FName> ItemIds;
	for (const FName RowName : Rows)
	{
		const FASLootPoolEntry* Entry = RandomPool->FindRow<FASLootPoolEntry>(RowName, TEXT("ResolveMonsterDropCandidates"), false);
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
		OutEntries.Add(*Entry);
	}
	return true;
}

bool FASMonsterDropGroupRow::Validate(FString& OutError) const
{
	OutError.Reset();
	if (!FMath::IsFinite(DropChance) || DropChance < 0.0f || DropChance > 1.0f
		|| MinRollCount < 0 || MaxRollCount < MinRollCount || MaxRollCount > 10000)
	{
		OutError = TEXT("Drop chance must be finite within [0, 1], and roll counts must satisfy 0 <= Min <= Max <= 10000.");
		return false;
	}
	if (!FASLootValidation::ValidateGrants(FixedItems, OutError)) return false;
	for (const auto& Weight : CategoryWeights)
	{
		if ((Weight.Key != EASItemType::Weapon && Weight.Key != EASItemType::Ammo
			&& Weight.Key != EASItemType::Consumable && Weight.Key != EASItemType::CraftingMaterial
			&& Weight.Key != EASItemType::Bag)
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
	TMap<FName, const UASItemDefinition*> ItemIds;
	for (const FASItemGrant& Grant : FixedItems)
	{
		ItemIds.Add(Grant.ItemDefinition->ItemId, Grant.ItemDefinition);
	}
	for (const FASLootPoolEntry& Entry : Entries)
	{
		const UASItemDefinition* const* Existing = ItemIds.Find(Entry.ItemDefinition->ItemId);
		if (Existing && *Existing != Entry.ItemDefinition)
		{
			OutError = TEXT("Fixed items and candidate items disagree on an ItemId.");
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult FASMonsterDropGroupRow::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (!Validate(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return EDataValidationResult::Valid;
}
#endif
