#include "Items/Loot/ASMonsterDropGenerator.h"

#include "Items/Data/ASItemDefinition.h"
#include "Items/Loot/ASLootValidation.h"
#include "Items/Loot/ASMonsterDropDefinition.h"

bool FASMonsterDropGenerator::Generate(const FASMonsterDropGroupRow& DropGroup, FRandomStream& Random,
	TArray<FASItemGrant>& OutItems, FString& OutError)
{
	OutItems.Reset();
	OutError.Reset();
	if (!DropGroup.Validate(OutError)) return false;

	FRandomStream Stream = Random;
	TArray<FASItemGrant> Result;
	TMap<const UASItemDefinition*, int64> Totals;
	TMap<const UASItemDefinition*, int32> ResultIndices;
	auto AddGrant = [&](UASItemDefinition* Item, const int32 Quantity)
	{
		int64& Total = Totals.FindOrAdd(Item);
		if (Total + Quantity > MAX_int32)
		{
			OutError = FString::Printf(TEXT("Loot quantity overflow for %s."), *Item->ItemId.ToString());
			return false;
		}
		Total += Quantity;
		if (DropGroup.bMergeIdenticalItems)
		{
			if (const int32* Index = ResultIndices.Find(Item))
			{
				Result[*Index].Quantity += Quantity;
				return true;
			}
		}
		FASItemGrant Grant;
		Grant.ItemDefinition = Item;
		Grant.Quantity = Quantity;
		ResultIndices.Add(Item, Result.Add(Grant));
		return true;
	};

	for (const FASItemGrant& Grant : DropGroup.FixedItems)
	{
		if (!AddGrant(Grant.ItemDefinition, Grant.Quantity)) return false;
	}

	const int32 Rolls = Stream.RandRange(DropGroup.MinRollCount, DropGroup.MaxRollCount);
	if (Rolls > 0)
	{
		TArray<FASLootPoolEntry> Entries;
		if (!DropGroup.ResolveRandomEntries(Entries, OutError)) return false;
		TArray<double> Weights;
		double LargestWeight = 0.0;
		bool bHadPositiveCandidate = false;
		for (const FASLootPoolEntry& Entry : Entries)
		{
			const UASItemDefinition* Item = Entry.ItemDefinition;
			double Weight = Entry.Weight;
			if (const float* Category = DropGroup.CategoryWeights.Find(Item->ItemType)) Weight *= *Category;
			for (const FASLootTagWeight& TagWeight : DropGroup.TagWeights)
			{
				if (Item->ItemTags.HasTag(TagWeight.ItemTag)) Weight *= TagWeight.Multiplier;
				if (!FMath::IsFinite(Weight)) { OutError = TEXT("Effective loot weight overflow."); return false; }
			}
			bHadPositiveCandidate |= Weight > 0.0;
			if (!DropGroup.bAllowRepeatedItems && Totals.Contains(Item)) Weight = 0.0;
			Weights.Add(Weight);
			LargestWeight = FMath::Max(LargestWeight, Weight);
		}
		if (LargestWeight <= 0.0)
		{
			if (!DropGroup.bAllowRepeatedItems && bHadPositiveCandidate)
			{
				if (!FASLootValidation::ValidateGrants(Result, OutError)) return false;
				Random = Stream;
				OutItems = MoveTemp(Result);
				return true;
			}
			OutError = TEXT("Random rolls have no eligible candidates with a positive weight.");
			return false;
		}
		for (double& Weight : Weights) Weight /= LargestWeight;
		for (int32 Roll = 0; Roll < Rolls; ++Roll)
		{
			double TotalWeight = 0.0;
			for (const double Weight : Weights) TotalWeight += Weight;
			if (TotalWeight <= 0.0) break;
			const double Draw = static_cast<double>(Stream.FRand()) * TotalWeight;
			double Cumulative = 0.0;
			int32 Selected = INDEX_NONE;
			int32 LastEligible = INDEX_NONE;
			for (int32 Index = 0; Index < Weights.Num(); ++Index)
			{
				if (Weights[Index] <= 0.0) continue;
				LastEligible = Index;
				Cumulative += Weights[Index];
				if (Draw < Cumulative) { Selected = Index; break; }
			}
			if (Selected == INDEX_NONE) Selected = LastEligible;
			const FASLootPoolEntry& Entry = Entries[Selected];
			if (!AddGrant(Entry.ItemDefinition, Stream.RandRange(Entry.MinQuantity, Entry.MaxQuantity))) return false;
			if (!DropGroup.bAllowRepeatedItems) Weights[Selected] = 0.0;
		}
	}
	if (!FASLootValidation::ValidateGrants(Result, OutError)) return false;
	Random = Stream;
	OutItems = MoveTemp(Result);
	return true;
}
