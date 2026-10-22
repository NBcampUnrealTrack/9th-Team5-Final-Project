#include "Items/Loot/ASLootDefinition.h"

#include "Items/Data/ASItemDefinition.h"
#include "Items/Loot/ASLootChest.h"
#include "Items/Loot/ASLootValidation.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Misc/DelayedAutoRegister.h"
#include "UObject/CoreRedirects.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	// Keep existing assets readable without retaining Common as a selectable tier.
	FDelayedAutoRegisterHelper RegisterLootRarityRedirect(EDelayedRegisterRunPhase::ObjectSystemReady, []
	{
		const FString EnumPath = TEXT("/Script/ApocalypseSurvival.EASLootRarity");
		FCoreRedirect Redirect(ECoreRedirectFlags::Type_Enum, EnumPath, EnumPath);
		Redirect.ValueChanges.Add(TEXT("Common"), TEXT("Uncommon"));
		Redirect.ValueChanges.Add(TEXT("EASLootRarity::Common"), TEXT("EASLootRarity::Uncommon"));
		const TArray<FCoreRedirect> Redirects = { Redirect };
		FCoreRedirects::AddRedirectList(Redirects, TEXT("ASItemRarity"));
	});
}

bool FASLootPoolEntry::Validate(FString& OutError) const
{
	OutError.Reset();
	if (!FASLootValidation::ValidateItem(ItemDefinition, OutError)) return false;
	if (!FMath::IsFinite(Weight) || Weight < 0.0f
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

FLinearColor FASLootRarityAppearanceRow::GetResolvedColor() const
{
	if (bOverrideColor) return ColorOverride;
	switch (Rarity)
	{
	case EASLootRarity::Uncommon: return FLinearColor(0.15f, 0.85f, 0.25f, 1.0f);
	case EASLootRarity::Rare: return FLinearColor(0.12f, 0.45f, 1.0f, 1.0f);
	case EASLootRarity::Epic: return FLinearColor(0.68f, 0.22f, 1.0f, 1.0f);
	case EASLootRarity::Legendary: return FLinearColor(1.0f, 0.48f, 0.08f, 1.0f);
	default: return FLinearColor(0.15f, 0.85f, 0.25f, 1.0f);
	}
}

bool FASLootRarityAppearanceRow::Validate(FString& OutError) const
{
	OutError.Reset();
	if (!IsValidLootRarity(Rarity))
	{
		OutError = TEXT("Appearance rarity must be Uncommon, Rare, Epic, or Legendary.");
		return false;
	}
	const bool bColorFinite = FMath::IsFinite(ColorOverride.R) && FMath::IsFinite(ColorOverride.G)
		&& FMath::IsFinite(ColorOverride.B) && FMath::IsFinite(ColorOverride.A);
	if (MaterialIndex < 0 || (bOverrideColor && !bColorFinite))
	{
		OutError = TEXT("Appearance material index must be nonnegative and color override must be finite.");
		return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult FASLootRarityAppearanceRow::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
#endif

bool FASLootChestPresetRow::Validate(FString& OutError) const
{
	OutError.Reset();
	if (!IsValidLootRarity(Rarity))
	{
		OutError = TEXT("Chest rarity must be Uncommon, Rare, Epic, or Legendary.");
		return false;
	}
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
	if (!FASLootValidation::ValidateGrants(FixedItems, OutError)) return false;
	for (const FASItemGrant& Grant : FixedItems)
	{
		Ids.Add(Grant.ItemDefinition->ItemId, Grant.ItemDefinition);
	}
	for (const auto& Weight : CategoryWeights)
	{
		if ((Weight.Key != EASItemType::Weapon && Weight.Key != EASItemType::Ammo
			&& Weight.Key != EASItemType::Consumable && Weight.Key != EASItemType::CraftingMaterial)
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
