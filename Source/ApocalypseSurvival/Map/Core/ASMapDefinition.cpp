#include "Map/Core/ASMapDefinition.h"

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

bool UASMapDefinition::ValidateDefinition(TArray<FString>& OutErrors) const
{
	const int32 InitialErrorCount = OutErrors.Num();
	const FString AssetName = GetName();

	if (!FMath::IsFinite(DangerIncreaseChance) || DangerIncreaseChance < 0.0f || DangerIncreaseChance > 1.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: DangerIncreaseChance is outside 0..1."), *AssetName));
	}

	if (MapWorldMax.X <= MapWorldMin.X || MapWorldMax.Y <= MapWorldMin.Y)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MapWorldMax must be greater than MapWorldMin."), *AssetName));
	}

	if (DangerColors.Num() != ASMap::MaxDangerLevel)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: DangerColors needs %d entries."), *AssetName, ASMap::MaxDangerLevel));
	}

	if (Vision.DefaultVisionRadius <= 0.0f || Vision.UpdateHz <= 0.0f || Vision.AttackRevealSeconds < 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: vision radius, update rate and reveal time must be positive."), *AssetName));
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

FVector UASMapDefinition::MapUVToWorld(const FVector2D& MapUV) const
{
	const FVector2D Size = GetMapWorldSize();
	return FVector(
		MapWorldMin.X + (1.0 - MapUV.Y) * Size.X,
		MapWorldMin.Y + MapUV.X * Size.Y,
		0.0);
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
