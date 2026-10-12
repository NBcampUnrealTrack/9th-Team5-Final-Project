#include "Items/Combat/ASWeaponAttackProfile.h"

#include "Items/Combat/ASWeaponAttackExecution.h"
#include "Skills/ASSkillTags.h"

UASMeleeAttackProfile::UASMeleeAttackProfile()
{
	ExecutionClass = UASMeleeAttackExecution::StaticClass();
	HitEventTag = ASGameplayTags::Event_Skill_Hit;
}

UASRangedAttackProfile::UASRangedAttackProfile()
{
	ExecutionClass = UASRangedAttackExecution::StaticClass();
	HitEventTag = ASGameplayTags::Event_Skill_Hit;
}
