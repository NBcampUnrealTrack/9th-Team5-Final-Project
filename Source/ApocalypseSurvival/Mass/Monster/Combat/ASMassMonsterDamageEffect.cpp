#include "Mass/Monster/Combat/ASMassMonsterDamageEffect.h"

#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Skills/ASSkillTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterDamageEffect)

UASMassMonsterDamageEffect::UASMassMonsterDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UASHealthAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = ASGameplayTags::Data_Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}
