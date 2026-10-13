#include "Items/Loot/ASLootDaySpawnSchedule.h"

#include "Items/Loot/ASLootDefinition.h"

bool FASLootDaySpawnScheduleRow::Validate(FString& OutError) const
{
	OutError.Reset();
	if (DayNumber < 1 || !FMath::IsFinite(SecondsAfterDayStart) || SecondsAfterDayStart < 0.0f)
	{
		OutError = TEXT("DayNumber must be positive and SecondsAfterDayStart must be finite and nonnegative.");
		return false;
	}
	if (!IsValid(LootPreset.DataTable) || LootPreset.RowName.IsNone()
		|| LootPreset.DataTable->GetRowStruct() != FASLootChestPresetRow::StaticStruct()
		|| !LootPreset.DataTable->FindRow<FASLootChestPresetRow>(LootPreset.RowName, TEXT("LootDaySpawnSchedule"), false))
	{
		OutError = TEXT("Select a valid loot preset table row.");
		return false;
	}
	if (Placement == EASLootDaySpawnPlacement::RandomArea)
	{
		if (RandomBoundsTransform.ContainsNaN() || RandomBoundsTransform.GetScale3D().GetMin() <= 0.0
			|| RandomBoundsHalfExtent.ContainsNaN() || RandomBoundsHalfExtent.GetMin() <= 0.0
			|| RandomSpawnSettings.Count < 1 || RandomSpawnSettings.Count > 10000
			|| RandomSpawnSettings.MaxAttempts < RandomSpawnSettings.Count || RandomSpawnSettings.MaxAttempts > 10000)
		{
			OutError = TEXT("Random placement requires positive bounds and valid count/attempt settings.");
			return false;
		}
	}
	else if (Placement == EASLootDaySpawnPlacement::FixedLocations)
	{
		if (FixedLocations.IsEmpty() || FixedLocations.Num() > 10000)
		{
			OutError = TEXT("Fixed placement requires 1..10000 locations.");
			return false;
		}
		for (const FTransform& Transform : FixedLocations)
		{
			if (Transform.ContainsNaN())
			{
				OutError = TEXT("A fixed location contains a non-finite transform.");
				return false;
			}
		}
	}
	else
	{
		OutError = TEXT("Select a supported placement mode.");
		return false;
	}
	return true;
}
