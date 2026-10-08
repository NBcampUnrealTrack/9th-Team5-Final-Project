#include "MapSystem/Zone/Effects/ASZoneEffectDefinition.h"

#include "GameplayEffect.h"
#include "GenericTeamAgentInterface.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FASZoneEffectRow* UASZoneEffectDefinition::FindBaseRow(uint8 TeamId) const
{
	return FindZoneRow(NAME_None, TeamId);
}

const FASZoneEffectRow* UASZoneEffectDefinition::FindZoneRow(FName ZoneId, uint8 TeamId) const
{
	if (!IsValid(ZoneEffectTable) || ZoneEffectTable->GetRowStruct() != FASZoneEffectRow::StaticStruct())
	{
		return nullptr;
	}

	const FASZoneEffectRow* Fallback = nullptr;
	for (const auto& Pair : ZoneEffectTable->GetRowMap())
	{
		const FASZoneEffectRow* Row = reinterpret_cast<const FASZoneEffectRow*>(Pair.Value);
		if (!Row || Row->ZoneId != ZoneId)
		{
			continue;
		}
		if (Row->TeamId == TeamId)
		{
			return Row;
		}
		if (Row->TeamId == FGenericTeamId::NoTeam.GetId())
		{
			Fallback = Row;
		}
	}
	return Fallback;
}

void UASZoneEffectDefinition::ValidateMagnitudes(const TArray<FASZoneEffectMagnitude>& Magnitudes,
	const FString& OwnerPath, TArray<FString>& OutErrors)
{
	for (int32 MagnitudeIndex = 0; MagnitudeIndex < Magnitudes.Num(); ++MagnitudeIndex)
	{
		const FASZoneEffectMagnitude& Magnitude = Magnitudes[MagnitudeIndex];
		const FString Path = FString::Printf(TEXT("%s.Magnitudes[%d]"), *OwnerPath, MagnitudeIndex);
		if (!Magnitude.DataTag.IsValid())
		{
			OutErrors.Add(Path + TEXT(" has an invalid or empty DataTag."));
		}
		if (Magnitude.ValuesByDangerLevel.Num() != ASMap::MaxDangerLevel)
		{
			OutErrors.Add(FString::Printf(TEXT("%s.ValuesByDangerLevel needs %d entries."), *Path, ASMap::MaxDangerLevel));
		}
		for (int32 ValueIndex = 0; ValueIndex < Magnitude.ValuesByDangerLevel.Num(); ++ValueIndex)
		{
			if (!FMath::IsFinite(Magnitude.ValuesByDangerLevel[ValueIndex]))
			{
				OutErrors.Add(FString::Printf(TEXT("%s.ValuesByDangerLevel[%d] must be finite."), *Path, ValueIndex));
			}
		}
	}
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
			TMap<FName, TSet<uint8>> ZoneTeams;
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
				TSet<uint8>& TeamsForZone = ZoneTeams.FindOrAdd(Row->ZoneId);
				if (TeamsForZone.Contains(Row->TeamId))
				{
					OutErrors.Add(FString::Printf(TEXT("%s has duplicate (ZoneId, TeamId) (%s, %u)."),
						*RowPath, *Row->ZoneId.ToString(), Row->TeamId));
				}
				TeamsForZone.Add(Row->TeamId);
				ValidateMagnitudes(Row->Magnitudes, RowPath, OutErrors);
			}
		}
	}
	if (!FMath::IsFinite(UpdateInterval) || UpdateInterval <= 0.0f)
	{
		OutErrors.Add(AssetName + TEXT(": UpdateInterval must be finite and greater than 0."));
	}
	return OutErrors.Num() == InitialErrorCount;
}

bool UASZoneEffectDefinition::GetMagnitudeForLevel(const FASZoneEffectMagnitude& Magnitude, int32 DangerLevel, float& OutValue)
{
	if (DangerLevel < ASMap::MinDangerLevel || DangerLevel > ASMap::MaxDangerLevel)
	{
		return false;
	}
	const int32 Index = DangerLevel - ASMap::MinDangerLevel;
	if (!Magnitude.ValuesByDangerLevel.IsValidIndex(Index) || !FMath::IsFinite(Magnitude.ValuesByDangerLevel[Index]))
	{
		return false;
	}
	OutValue = Magnitude.ValuesByDangerLevel[Index];
	return true;
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
