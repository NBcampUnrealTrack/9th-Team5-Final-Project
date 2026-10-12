#include "Items/GAS/ASItemEffects.h"

#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Skills/ASSkillTags.h"

UASItemDamageEffect::UASItemDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UASHealthAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = ASGameplayTags::Data_Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}
