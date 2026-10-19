#include "Modules/Data/ASModuleBalanceRows.h"

#include "Abilities/GameplayAbility.h"
#include "GAS/ASAbilitySet.h"
#include "GameplayEffect.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FName FASModuleBalanceRow::GetCoreEffectIdentity() const
{
	return CoreEffectId.IsNone() ? FName(StaticEnum<EASModuleStat>()->GetNameStringByValue(static_cast<int64>(CoreStat))) : CoreEffectId;
}

bool FASModuleBalanceRow::Validate(FString& OutError) const
{
	OutError.Reset();
	if (!FMath::IsFinite(CoreValue) || static_cast<uint8>(Category) > static_cast<uint8>(EASModuleCategory::Utility)
		|| static_cast<uint8>(CoreStat) > static_cast<uint8>(EASModuleStat::MoveSpeed))
	{
		OutError = TEXT("Module category, core stat, or core value is invalid.");
		return false;
	}
	if (CoreStat == EASModuleStat::None && (AbilitySet.IsNull() || CoreEffectId.IsNone()))
	{
		OutError = TEXT("A module requires a CoreStat, or a special AbilitySet with a CoreEffectId.");
		return false;
	}
	if (AbilitySet.IsNull()) return true;
	const UASAbilitySet* Set = AbilitySet.LoadSynchronous();
	if (!Set || (Set->GetGrantedAbilities().IsEmpty() && Set->GetGrantedEffects().IsEmpty()))
	{
		OutError = TEXT("The special module AbilitySet must exist and contain an ability or effect.");
		return false;
	}
	for (const FASAbilitySet_GameplayAbility& Entry : Set->GetGrantedAbilities())
	{
		if (!Entry.Ability || Entry.Ability->HasAnyClassFlags(CLASS_Abstract) || Entry.AbilityLevel < 1)
		{
			OutError = TEXT("Module abilities require a concrete class and a positive level.");
			return false;
		}
	}
	for (const FASAbilitySet_GameplayEffect& Entry : Set->GetGrantedEffects())
	{
		const UGameplayEffect* Effect = Entry.GameplayEffect ? Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (!Effect || !FMath::IsFinite(Entry.EffectLevel) || Entry.EffectLevel <= 0.0f
			|| Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
			|| Effect->Period.GetValueAtLevel(Entry.EffectLevel) != 0.0f
			|| Effect->GetStackingType() != EGameplayEffectStackingType::None)
		{
			OutError = TEXT("Equipment effects must be Infinite, non-periodic, non-stacking, and have a positive level. Use abilities for triggered effects.");
			return false;
		}
	}
	return true;
}

bool FASModuleOptionBalanceRow::Validate(FString& OutError) const
{
	OutError.Reset();
	if (Stat == EASModuleStat::None || static_cast<uint8>(Stat) > static_cast<uint8>(EASModuleStat::MoveSpeed)
		|| !FMath::IsFinite(MinValue) || !FMath::IsFinite(MaxValue) || MinValue > MaxValue
		|| !FMath::IsFinite(Weight) || Weight < 0.0f)
	{
		OutError = TEXT("Option requires a stat, finite MinValue <= MaxValue, and a finite non-negative Weight.");
		return false;
	}
	return true;
}

#if WITH_EDITOR
namespace
{
template <typename T>
EDataValidationResult ValidateRow(const T& Row, FDataValidationContext& Context)
{
	FString Error;
	if (!Row.Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
}
EDataValidationResult FASModuleBalanceRow::IsDataValid(FDataValidationContext& Context) const { return ValidateRow(*this, Context); }
EDataValidationResult FASModuleOptionBalanceRow::IsDataValid(FDataValidationContext& Context) const { return ValidateRow(*this, Context); }
#endif