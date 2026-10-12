#include "Items/Combat/ASWeaponAttackProfile.h"

#include "Items/Combat/ASWeaponAttackExecution.h"
#include "Skills/ASSkillTags.h"

UASMeleeAttackProfile::UASMeleeAttackProfile()
{
	ExecutionClass = UASMeleeAttackExecution::StaticClass();
	TargetMode = EASAttackTargetMode::Area;
	HitEventTag = ASGameplayTags::Event_Skill_Hit;
}

UASRangedAttackProfile::UASRangedAttackProfile()
{
	ExecutionClass = UASRangedAttackExecution::StaticClass();
	TargetMode = EASAttackTargetMode::Single;
	HitEventTag = ASGameplayTags::Event_Skill_Hit;
}
