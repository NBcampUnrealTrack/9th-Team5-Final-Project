#include "Modules/Data/ASModuleDefinition.h"

#include "Abilities/GameplayAbility.h"
#include "GAS/ASAbilitySet.h"
#include "GameplayEffect.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UASModuleDefinition::ValidateDefinition(FString& OutError) const
{
	OutError.Reset();
	if (ModuleId.IsNone() || !IsValid(AbilitySet))
	{
		OutError = TEXT("ModuleId and AbilitySet must be set.");
		return false;
	}
	if (AbilitySet->GetGrantedAbilities().IsEmpty() && AbilitySet->GetGrantedEffects().IsEmpty())
	{
		OutError = TEXT("The module AbilitySet must contain at least one ability or effect.");
		return false;
	}
	for (const FASAbilitySet_GameplayAbility& Entry : AbilitySet->GetGrantedAbilities())
	{
		if (!Entry.Ability || Entry.Ability->HasAnyClassFlags(CLASS_Abstract) || Entry.AbilityLevel < 1)
		{
			OutError = TEXT("Module abilities must have a class and a positive level.");
			return false;
		}
	}
	for (const FASAbilitySet_GameplayEffect& Entry : AbilitySet->GetGrantedEffects())
	{
		const UGameplayEffect* Effect = Entry.GameplayEffect ? Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (!Effect || !FMath::IsFinite(Entry.EffectLevel) || Entry.EffectLevel <= 0.0f
			|| Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
			|| Effect->Period.GetValueAtLevel(Entry.EffectLevel) != 0.0f
			|| Effect->GetStackingType() != EGameplayEffectStackingType::None)
		{
			OutError = TEXT("Module effects must be Infinite, non-periodic, non-stacking, and have a positive level. Use abilities for triggered effects.");
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UASModuleDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateDefinition(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
