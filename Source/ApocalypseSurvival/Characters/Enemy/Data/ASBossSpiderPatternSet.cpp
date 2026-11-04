#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"

#if WITH_EDITOR

#include "Characters/Enemy/BossSpider/ASBossSpiderEgg.h"
#include <Characters/Enemy/BossSpider/ASBossSpiderWebTrap.h>

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Misc/DataValidation.h"

EDataValidationResult UASBossSpiderPatternSet::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    auto AddError = [&Context, &Result](const FString& Message)
    {
        Context.AddError(FText::FromString(Message));
        Result = EDataValidationResult::Invalid;
    };

    FASResolvedMonsterPatternSet ResolvedSet;
    if (!ResolvePatternSet(ResolvedSet))
    {
        return EDataValidationResult::Invalid;
    }

    if (OverlapResolution.PlayerSeparationPadding < 0.0f)
    {
        AddError(TEXT("Boss Spider OverlapResolution PlayerSeparationPadding cannot be negative."));
    }
    if (OverlapResolution.MaxResolutionPasses < 1 || OverlapResolution.MaxResolutionPasses > 4)
    {
        AddError(TEXT("Boss Spider OverlapResolution MaxResolutionPasses must be in [1, 4]."));
    }

	const FASBossSpiderStateTreeConfig* StateTreeConfig =
		ResolvedSet.SelectionPolicyConfig.GetPtr<FASBossSpiderStateTreeConfig>();
	if (!StateTreeConfig)
    {
		AddError(TEXT("Boss Spider PatternSet requires FASBossSpiderStateTreeConfig in SelectionPolicyConfig."));
    }
    else
    {
		if (StateTreeConfig->DecisionInterval <= 0.0f)
        {
			AddError(TEXT("Boss Spider StateTree DecisionInterval must be greater than zero."));
        }
		if (!StateTreeConfig->FanPatternId.IsValid()
			|| !StateTreeConfig->ChargePatternId.IsValid()
			|| !StateTreeConfig->WebTrapPatternId.IsValid()
			|| !StateTreeConfig->EggSummonPatternId.IsValid()
			|| !StateTreeConfig->CeilingSlamPatternId.IsValid()
			|| !StateTreeConfig->CaptureChargePatternId.IsValid())
        {
			AddError(TEXT("Boss Spider StateTree pattern IDs must all be configured."));
        }

		const auto WarnIfStateTreePatternIsNotMigrated = [&ResolvedSet, &Context](const FGameplayTag PatternId,
            const TCHAR* PatternLabel)
        {
            if (PatternId.IsValid() && !ResolvedSet.FindPattern(PatternId))
            {
                Context.AddWarning(FText::FromString(FString::Printf(
					TEXT("Boss Spider StateTree %s '%s' has no enabled PatternEntry yet. This is allowed during incremental pattern migration."),
                    PatternLabel, *PatternId.ToString())));
            }
        };
		WarnIfStateTreePatternIsNotMigrated(StateTreeConfig->FanPatternId, TEXT("FanPatternId"));
		WarnIfStateTreePatternIsNotMigrated(StateTreeConfig->ChargePatternId, TEXT("ChargePatternId"));
		WarnIfStateTreePatternIsNotMigrated(StateTreeConfig->WebTrapPatternId, TEXT("WebTrapPatternId"));
		WarnIfStateTreePatternIsNotMigrated(StateTreeConfig->EggSummonPatternId, TEXT("EggSummonPatternId"));
		WarnIfStateTreePatternIsNotMigrated(StateTreeConfig->CeilingSlamPatternId, TEXT("CeilingSlamPatternId"));
		WarnIfStateTreePatternIsNotMigrated(StateTreeConfig->CaptureChargePatternId, TEXT("CaptureChargePatternId"));
    }

    for (const FASMonsterPatternEntry& Entry : ResolvedSet.PatternEntries)
    {
        if (!Entry.bEnabled)
        {
            continue;
        }

        if (const FASBossSpiderWebTrapConfig* WebTrapConfig =
            Entry.RuntimeConfig.GetPtr<FASBossSpiderWebTrapConfig>())
        {
            if (!WebTrapConfig->TrapActorClass)
            {
                AddError(FString::Printf(TEXT("WebTrap pattern '%s' requires TrapActorClass."),
                    *Entry.PatternId.ToString()));
            }
            if (!WebTrapConfig->WebBoundEffectClass)
            {
                AddError(FString::Printf(TEXT("WebTrap pattern '%s' requires WebBoundEffectClass."),
                    *Entry.PatternId.ToString()));
            }
            if (WebTrapConfig->BoundDuration + KINDA_SMALL_NUMBER
                < WebTrapConfig->EscapeInputWindow + WebTrapConfig->EscapeInputGrace)
            {
                AddError(FString::Printf(TEXT("WebTrap pattern '%s' has BoundDuration shorter than its escape window plus grace."),
                    *Entry.PatternId.ToString()));
            }
            if (WebTrapConfig->MaxTrapCount < 1 || WebTrapConfig->MaxTrapCount > 4)
            {
                AddError(FString::Printf(TEXT("WebTrap pattern '%s' MaxTrapCount must be in [1, 4]."),
                    *Entry.PatternId.ToString()));
            }
        }

        if (const FASBossSpiderEggSummonConfig* EggConfig =
            Entry.RuntimeConfig.GetPtr<FASBossSpiderEggSummonConfig>())
        {
            if (!EggConfig->EggActorClass)
            {
                AddError(FString::Printf(TEXT("EggSummon pattern '%s' requires EggActorClass."),
                    *Entry.PatternId.ToString()));
            }
            if (!EggConfig->EliteMinionClass)
            {
                AddError(FString::Printf(TEXT("EggSummon pattern '%s' requires EliteMinionClass."),
                    *Entry.PatternId.ToString()));
            }
            if (EggConfig->BaseEggCount < 1 || EggConfig->MaxEggCount < EggConfig->BaseEggCount
                || EggConfig->MaxEggCount > 6)
            {
                AddError(FString::Printf(TEXT("EggSummon pattern '%s' requires 1 <= BaseEggCount <= MaxEggCount <= 6."),
                    *Entry.PatternId.ToString()));
            }
            if (EggConfig->SpawnMinRadius > EggConfig->SpawnMaxRadius)
            {
                AddError(FString::Printf(TEXT("EggSummon pattern '%s' SpawnMinRadius cannot exceed SpawnMaxRadius."),
                    *Entry.PatternId.ToString()));
            }
            if (EggConfig->MinEggSpacing <= 0.0f || EggConfig->HatchDelay <= 0.0f)
            {
                AddError(FString::Printf(TEXT("EggSummon pattern '%s' requires positive MinEggSpacing and HatchDelay."),
                    *Entry.PatternId.ToString()));
            }
        }

        if (const FASBossSpiderCeilingSlamConfig* CeilingSlamConfig =
            Entry.RuntimeConfig.GetPtr<FASBossSpiderCeilingSlamConfig>())
        {
            if (!CeilingSlamConfig->IndicatorClass)
            {
                AddError(FString::Printf(TEXT("CeilingSlam pattern '%s' requires IndicatorClass."),
                    *Entry.PatternId.ToString()));
            }
            if (!CeilingSlamConfig->DamageEffectClass)
            {
                AddError(FString::Printf(TEXT("CeilingSlam pattern '%s' requires DamageEffectClass."),
                    *Entry.PatternId.ToString()));
            }
            if (!CeilingSlamConfig->CeilingUntargetableEffectClass)
            {
                AddError(FString::Printf(TEXT("CeilingSlam pattern '%s' requires CeilingUntargetableEffectClass."),
                    *Entry.PatternId.ToString()));
            }
            if (CeilingSlamConfig->LandingRadius <= 0.0f || CeilingSlamConfig->ShockwaveRadius <= 0.0f
                || CeilingSlamConfig->RiseDuration <= 0.0f || CeilingSlamConfig->LandingTelegraphDuration <= 0.0f)
            {
                AddError(FString::Printf(TEXT("CeilingSlam pattern '%s' requires positive RiseDuration, LandingTelegraphDuration, LandingRadius and ShockwaveRadius."),
                    *Entry.PatternId.ToString()));
            }
            if (CeilingSlamConfig->LandingCapsuleClearance < 0.0f)
            {
                AddError(FString::Printf(TEXT("CeilingSlam pattern '%s' LandingCapsuleClearance cannot be negative."),
                    *Entry.PatternId.ToString()));
            }
            if (CeilingSlamConfig->ShockwaveCount < 0 || CeilingSlamConfig->ShockwaveCount > 3)
            {
                AddError(FString::Printf(TEXT("CeilingSlam pattern '%s' ShockwaveCount must be in [0, 3]."),
                    *Entry.PatternId.ToString()));
            }
        }

        if (const FASBossSpiderFanConfig* FanConfig = Entry.RuntimeConfig.GetPtr<FASBossSpiderFanConfig>())
        {
            if (!FanConfig->IndicatorClass)
            {
                AddError(FString::Printf(TEXT("Fan pattern '%s' requires a Sector IndicatorClass."),
                    *Entry.PatternId.ToString()));
            }
            if (!FanConfig->DamageEffectClass)
            {
                AddError(FString::Printf(TEXT("Fan pattern '%s' requires DamageEffectClass."),
                    *Entry.PatternId.ToString()));
            }
        }
    }

    return Result;
}

#endif
