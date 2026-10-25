#include "MapSystem/Vision/ASVisionDefinition.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "GAS/ASAbilityTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UASVisionDefinition::UASVisionDefinition()
{
	AutoVisionSourceClasses = { AASPlayerCharacter::StaticClass() };
	AutoVisibilityTargetClasses = { AASMonsterCharacter::StaticClass() };
	DeadStateTag = ASGameplayTags::State_Dead;
	AttackRevealAbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
}

bool UASVisionDefinition::ValidateDefinition(TArray<FString>& OutErrors) const
{
	const int32 InitialErrorCount = OutErrors.Num();
	const FString AssetName = GetName();

	if (!FMath::IsFinite(Settings.DayVisionRadius) || Settings.DayVisionRadius <= 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: DayVisionRadius must be positive."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.NightVisionRadius) || Settings.NightVisionRadius <= 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: NightVisionRadius must be positive."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.DayNightBlendSeconds) || Settings.DayNightBlendSeconds < 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: DayNightBlendSeconds must be non-negative."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.UpdateHz) || Settings.UpdateHz <= 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: UpdateHz must be positive."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.AttackRevealSeconds) || Settings.AttackRevealSeconds < 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: AttackRevealSeconds must be non-negative."), *AssetName));
	}

	if (Settings.FogEdgeBlurTexels < 0 || Settings.FogEdgeBlurTexels > 8)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: FogEdgeBlurTexels must be in 0..8."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.FogFadeSeconds) || Settings.FogFadeSeconds < 0.0f || Settings.FogFadeSeconds > 2.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: FogFadeSeconds must be finite and in 0..2."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.MassVisibilityBufferDistance) || Settings.MassVisibilityBufferDistance < 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MassVisibilityBufferDistance must be finite and non-negative."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.MassVisibilityExitInsetDistance) || Settings.MassVisibilityExitInsetDistance < 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MassVisibilityExitInsetDistance must be finite and non-negative."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.MassVisibilityReentryInsetDistance) || Settings.MassVisibilityReentryInsetDistance < 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MassVisibilityReentryInsetDistance must be finite and non-negative."), *AssetName));
	}
	else if (FMath::IsFinite(Settings.MassVisibilityExitInsetDistance)
		&& Settings.MassVisibilityReentryInsetDistance < Settings.MassVisibilityExitInsetDistance)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MassVisibilityReentryInsetDistance must be greater than or equal to MassVisibilityExitInsetDistance."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.MassVisibilityFadeInSeconds) || Settings.MassVisibilityFadeInSeconds < 0.0f
		|| Settings.MassVisibilityFadeInSeconds > 1.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MassVisibilityFadeInSeconds must be finite and in 0..1."), *AssetName));
	}

	if (!FMath::IsFinite(Settings.MassVisibilityCullDelaySeconds) || Settings.MassVisibilityCullDelaySeconds < 0.0f
		|| Settings.MassVisibilityCullDelaySeconds > 2.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MassVisibilityCullDelaySeconds must be finite and in 0..2."), *AssetName));
	}

	return OutErrors.Num() == InitialErrorCount;
}

#if WITH_EDITOR
EDataValidationResult UASVisionDefinition::IsDataValid(FDataValidationContext& Context) const
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
