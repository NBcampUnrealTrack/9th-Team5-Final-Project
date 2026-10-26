#include "MapSystem/Zone/Effects/ASZoneEffectDefinition.h"

#include "Core/Team/ASTeamTypes.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FASZoneEffectRow::GetValueForLevel(int32 DangerLevel, float& OutValue) const
{
	if (DangerLevel < ASMap::MinDangerLevel || DangerLevel > ASMap::MaxDangerLevel)
	{
		return false;
	}
	const float Values[] = { Level1, Level2, Level3, Level4, Level5 };
	const float Value = Values[DangerLevel - ASMap::MinDangerLevel];
	if (!FMath::IsFinite(Value))
	{
		return false;
	}
	OutValue = Value;
	return true;
}

EASZoneEffectTarget UASZoneEffectDefinition::GetTargetForTeam(uint8 TeamId)
{
	if (TeamId == ASTeam::PlayerTeamId)
	{
		return EASZoneEffectTarget::Players;
	}
	if (TeamId == ASTeam::MonsterTeamId)
	{
		return EASZoneEffectTarget::Monsters;
	}
	return EASZoneEffectTarget::All;
}

bool UASZoneEffectDefinition::FindRows(FName ZoneId, EASZoneEffectTarget Target,
	TArray<const FASZoneEffectRow*>& OutRows) const
{
	OutRows.Reset();
	if (!IsValid(ZoneEffectTable) || ZoneEffectTable->GetRowStruct() != FASZoneEffectRow::StaticStruct())
	{
		return false;
	}

	TArray<const FASZoneEffectRow*> FallbackRows;
	for (const auto& Pair : ZoneEffectTable->GetRowMap())
	{
		const FASZoneEffectRow* Row = reinterpret_cast<const FASZoneEffectRow*>(Pair.Value);
		if (!Row || Row->ZoneId != ZoneId)
		{
			continue;
		}
		if (Row->Target == Target)
		{
			OutRows.Add(Row);
		}
		else if (Row->Target == EASZoneEffectTarget::All)
		{
			FallbackRows.Add(Row);
		}
	}
	if (OutRows.IsEmpty())
	{
		OutRows = MoveTemp(FallbackRows);
	}
	return !OutRows.IsEmpty();
}

bool UASZoneEffectDefinition::ValidateDefinition(TArray<FString>& OutErrors) const
{
	const int32 InitialErrorCount = OutErrors.Num();
	const FString AssetName = GetName();
	if (ZoneEffectTable)
	{
		if (ZoneEffectTable->GetRowStruct() != FASZoneEffectRow::StaticStruct())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: ZoneEffectTable '%s' RowStruct must be FASZoneEffectRow."),
				*AssetName, *ZoneEffectTable->GetName()));
		}
		else
		{
			// 같은 구역/대상의 행은 하나의 GE와 중복 없는 태그를 공유해야 한다.
			struct FGroupValidation
			{
				const FASZoneEffectRow* FirstRow = nullptr;
				TSet<FGameplayTag> DataTags;
				int32 RowCount = 0;
				bool bHasEmptyDataTag = false;
			};
			TMap<FName, TMap<EASZoneEffectTarget, FGroupValidation>> Groups;
			for (const auto& Pair : ZoneEffectTable->GetRowMap())
			{
				const FString RowPath = FString::Printf(TEXT("%s: ZoneEffectTable[%s]"), *AssetName, *Pair.Key.ToString());
				const FASZoneEffectRow* Row = reinterpret_cast<const FASZoneEffectRow*>(Pair.Value);
				if (!Row)
				{
					OutErrors.Add(RowPath + TEXT(" has no row data."));
					continue;
				}
				if (!Row->Effect)
				{
					OutErrors.Add(RowPath + TEXT(" has no Effect."));
				}
				else if (Row->Effect.GetDefaultObject()->DurationPolicy != EGameplayEffectDurationType::Infinite)
				{
					OutErrors.Add(RowPath + TEXT(" Effect must have Infinite duration (periodic effects are allowed)."));
				}
				FGroupValidation& Group = Groups.FindOrAdd(Row->ZoneId).FindOrAdd(Row->Target);
				if (Group.FirstRow && Group.FirstRow->Effect != Row->Effect)
				{
					OutErrors.Add(RowPath + TEXT(" has different Effects in the same (ZoneId, Target) group."));
				}
				if (!Group.FirstRow)
				{
					Group.FirstRow = Row;
				}
				++Group.RowCount;
				if (Row->DataTag.IsValid())
				{
					if (Group.DataTags.Contains(Row->DataTag))
					{
						OutErrors.Add(RowPath + TEXT(" has duplicate DataTag in the same (ZoneId, Target) group."));
					}
					Group.DataTags.Add(Row->DataTag);
				}
				else
				{
					Group.bHasEmptyDataTag = true;
				}
				for (int32 Level = ASMap::MinDangerLevel; Level <= ASMap::MaxDangerLevel; ++Level)
				{
					float Value = 0.0f;
					if (!Row->GetValueForLevel(Level, Value))
					{
						OutErrors.Add(FString::Printf(TEXT("%s.Level%d must be finite."), *RowPath, Level));
					}
				}
			}
			for (const auto& ZoneGroup : Groups)
			{
				for (const auto& TargetGroup : ZoneGroup.Value)
				{
					if (TargetGroup.Value.bHasEmptyDataTag && TargetGroup.Value.RowCount > 1)
					{
						OutErrors.Add(FString::Printf(TEXT("%s: ZoneEffectTable group (ZoneId=%s, Target=%s) has an empty DataTag with more than one row."),
							*AssetName, *ZoneGroup.Key.ToString(), *StaticEnum<EASZoneEffectTarget>()->GetNameStringByValue(static_cast<int64>(TargetGroup.Key))));
					}
				}
			}
		}
	}
	if (!FMath::IsFinite(UpdateInterval) || UpdateInterval <= 0.0f)
	{
		OutErrors.Add(AssetName + TEXT(": UpdateInterval must be finite and greater than 0."));
	}
	return OutErrors.Num() == InitialErrorCount;
}

#if WITH_EDITOR
EDataValidationResult UASZoneEffectDefinition::IsDataValid(FDataValidationContext& Context) const
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
