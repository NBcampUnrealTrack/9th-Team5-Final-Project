#include "GAS/Effects/ASMonsterDamageGameplayEffect.h"

#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Skills/ASSkillTags.h"

UASMonsterDamageGameplayEffect::UASMonsterDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& HealthModifier = Modifiers.AddDefaulted_GetRef();
	HealthModifier.Attribute = UASHealthAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat DamageMagnitude;
	DamageMagnitude.DataTag = ASGameplayTags::Data_Damage;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageMagnitude);
}
