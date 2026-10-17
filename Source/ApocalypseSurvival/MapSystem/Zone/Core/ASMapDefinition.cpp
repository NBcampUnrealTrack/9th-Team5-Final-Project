#include "MapSystem/Zone/Core/ASMapDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UASMapDefinition::UASMapDefinition()
{
	DangerColors = {
		FLinearColor(0.20f, 0.80f, 0.30f, 1.0f),
		FLinearColor(0.65f, 0.85f, 0.20f, 1.0f),
		FLinearColor(1.00f, 0.80f, 0.10f, 1.0f),
		FLinearColor(1.00f, 0.45f, 0.10f, 1.0f),
		FLinearColor(0.90f, 0.10f, 0.10f, 1.0f)
	};
}

const FASZoneDangerScheduleRow* UASMapDefinition::FindDangerScheduleRow(int32 Day) const
{
	if (!IsValid(DangerScheduleTable) || DangerScheduleTable->GetRowStruct() != FASZoneDangerScheduleRow::StaticStruct())
	{
		return nullptr;
	}

	const FASZoneDangerScheduleRow* Found = nullptr;
	for (const auto& Pair : DangerScheduleTable->GetRowMap())
	{
		const FASZoneDangerScheduleRow* Row = reinterpret_cast<const FASZoneDangerScheduleRow*>(Pair.Value);
		if (Row && Row->Day <= Day && (!Found || Row->Day > Found->Day))
		{
			Found = Row;
		}
	}
	return Found;
}

bool UASMapDefinition::ValidateDefinition(TArray<FString>& OutErrors) const
{
	const int32 InitialErrorCount = OutErrors.Num();
	const FString AssetName = GetName();

	if (!IsValid(DangerScheduleTable))
	{
		OutErrors.Add(AssetName + TEXT(": DangerScheduleTable is required."));
	}
	else if (DangerScheduleTable->GetRowStruct() != FASZoneDangerScheduleRow::StaticStruct())
	{
		OutErrors.Add(AssetName + TEXT(": DangerScheduleTable RowStruct must be FASZoneDangerScheduleRow."));
	}
	else
	{
		TSet<int32> Days;
		if (DangerScheduleTable->GetRowMap().IsEmpty())
		{
			OutErrors.Add(AssetName + TEXT(": DangerScheduleTable needs at least one row."));
		}
		for (const auto& Pair : DangerScheduleTable->GetRowMap())
		{
			const FString RowPath = FString::Printf(TEXT("%s: DangerScheduleTable[%s]"), *AssetName, *Pair.Key.ToString());
			const FASZoneDangerScheduleRow* Row = reinterpret_cast<const FASZoneDangerScheduleRow*>(Pair.Value);
			if (!Row)
			{
				OutErrors.Add(RowPath + TEXT(" has no row data."));
				continue;
			}
			if (Row->Day < 1)
			{
				OutErrors.Add(RowPath + TEXT(" Day must be >= 1."));
			}
			if (Days.Contains(Row->Day))
			{
				OutErrors.Add(RowPath + TEXT(" has duplicate Day."));
			}
			Days.Add(Row->Day);
			if (Row->MaxRaisedZonesDay < 0 || Row->MaxRaisedZonesNight < 0)
			{
				OutErrors.Add(RowPath + TEXT(" MaxRaisedZonesDay and MaxRaisedZonesNight must be >= 0."));
			}
			if (Row->MonsterCountThreshold < 0)
			{
				OutErrors.Add(RowPath + TEXT(" MonsterCountThreshold must be >= 0."));
			}
			if (!FMath::IsFinite(Row->BaseChance) || Row->BaseChance < 0.0f || Row->BaseChance > 1.0f)
			{
				OutErrors.Add(RowPath + TEXT(" BaseChance must be finite and in 0..1."));
			}
			if (!FMath::IsFinite(Row->PlayerZoneChance) || Row->PlayerZoneChance < 0.0f || Row->PlayerZoneChance > 1.0f)
			{
				OutErrors.Add(RowPath + TEXT(" PlayerZoneChance must be finite and in 0..1."));
			}
		}
		if (!Days.Contains(1))
		{
			OutErrors.Add(AssetName + TEXT(": DangerScheduleTable requires a Day 1 row."));
		}
	}

	if (MapWorldMax.X <= MapWorldMin.X || MapWorldMax.Y <= MapWorldMin.Y)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MapWorldMax must be greater than MapWorldMin."), *AssetName));
	}

	if (DangerColors.Num() != ASMap::MaxDangerLevel)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: DangerColors needs %d entries."), *AssetName, ASMap::MaxDangerLevel));
	}

	return OutErrors.Num() == InitialErrorCount;
}

FLinearColor UASMapDefinition::GetDangerColor(int32 DangerLevel) const
{
	if (DangerColors.Num() == 0)
	{
		return FLinearColor::White;
	}

	const int32 Index = FMath::Clamp(DangerLevel - ASMap::MinDangerLevel, 0, DangerColors.Num() - 1);
	return DangerColors[Index];
}

FVector2D UASMapDefinition::WorldToMapUV(const FVector& WorldLocation) const
{
	const FVector2D Size = GetMapWorldSize();
	return FVector2D(
		(WorldLocation.Y - MapWorldMin.Y) / Size.Y,
		1.0 - (WorldLocation.X - MapWorldMin.X) / Size.X);
}

#if WITH_EDITOR
EDataValidationResult UASMapDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FString> Errors;
	if (!ValidateDefinition(Errors))
	{
		for (const FString& Error : Errors)
		{
			Context.AddError(FText::FromString(Error));
		}
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
