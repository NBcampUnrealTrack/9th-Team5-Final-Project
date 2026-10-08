#include "GAS/Characters/Enemies/Abilities/ASMonsterRadialAttackAbility.h"

#include "Characters/Enemy/Data/ASMonsterTags.h"

UASMonsterRadialAttackAbility::UASMonsterRadialAttackAbility()
{
	Radius = 320.0f;
	ForwardOffset = 0.0f;
	Angle = 360.0f;
	AbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack_Radial);
}
