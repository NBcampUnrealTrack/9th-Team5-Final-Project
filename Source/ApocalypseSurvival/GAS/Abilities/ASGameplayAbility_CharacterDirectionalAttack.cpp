#include "GAS/Abilities/ASGameplayAbility_CharacterDirectionalAttack.h"

#include "Skills/ASSkillTags.h"

FGameplayTag UASGameplayAbility_CharacterDirectionalAttack::GetTargetingStateTag() const
{
	return ASGameplayTags::State_Targeting_DirectionalAttack;
}
