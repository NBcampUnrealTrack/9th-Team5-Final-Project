#include "Characters/Enemy/Data/ASMonsterTags.h"

namespace ASGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack, "Ability.Monster.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Melee, "Ability.Monster.Attack.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Ranged, "Ability.Monster.Attack.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Targeted, "Ability.Monster.Attack.Targeted");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Charge, "Ability.Monster.Attack.Charge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Radial, "Ability.Monster.Attack.Radial");

	UE_DEFINE_GAMEPLAY_TAG(State_Monster_Dead, "State.Monster.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_HitReact, "State.Monster.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_Attacking, "State.Monster.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_TargetedAttacking, "State.Monster.TargetedAttacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_Charging, "State.Monster.Charging");
}
