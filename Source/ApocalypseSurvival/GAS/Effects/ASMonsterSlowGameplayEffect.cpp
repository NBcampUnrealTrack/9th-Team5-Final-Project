#include "GAS/Effects/ASMonsterSlowGameplayEffect.h"

#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"

UASMonsterSlowGameplayEffect::UASMonsterSlowGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo& MoveSpeedModifier = Modifiers.AddDefaulted_GetRef();
	MoveSpeedModifier.Attribute = UASCombatAttributeSet::GetMoveSpeedAttribute();
	MoveSpeedModifier.ModifierOp = EGameplayModOp::Multiplicitive;
	MoveSpeedModifier.ModifierMagnitude = FScalableFloat(0.5f);

	InheritableOwnedTagsContainer.AddTag(ASGameplayTags::Debuff_Movement_Slow_EliteB);
}
