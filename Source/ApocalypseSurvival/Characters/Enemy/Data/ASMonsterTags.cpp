#include "Characters/Enemy/Data/ASMonsterTags.h"

namespace ASGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack, "Ability.Monster.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Melee, "Ability.Monster.Attack.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Ranged, "Ability.Monster.Attack.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Targeted, "Ability.Monster.Attack.Targeted");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Charge, "Ability.Monster.Attack.Charge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Slam, "Ability.Monster.Attack.Slam");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Combo, "Ability.Monster.Attack.Combo");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_HazardSequence, "Ability.Monster.Attack.HazardSequence");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_RadialVolley, "Ability.Monster.Attack.RadialVolley");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_RadialBurstCascade, "Ability.Monster.Attack.RadialBurstCascade");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_Radial, "Ability.Monster.Attack.Radial");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_SlowField, "Ability.Monster.Attack.SlowField");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Monster_Attack_SniperBeam, "Ability.Monster.Attack.SniperBeam");

	UE_DEFINE_GAMEPLAY_TAG(Event_Monster_ComboStep_ChargeFinished, "Event.Monster.ComboStep.ChargeFinished");
	UE_DEFINE_GAMEPLAY_TAG(Event_Monster_ComboStep_SlamFinished, "Event.Monster.ComboStep.SlamFinished");
	UE_DEFINE_GAMEPLAY_TAG(Event_Monster_ComboStep_TargetedFinished, "Event.Monster.ComboStep.TargetedFinished");
	UE_DEFINE_GAMEPLAY_TAG(Event_Monster_ComboStep_RadialFinished, "Event.Monster.ComboStep.RadialFinished");
	UE_DEFINE_GAMEPLAY_TAG(Event_Monster_ComboStep_HazardSequenceFinished, "Event.Monster.ComboStep.HazardSequenceFinished");
	UE_DEFINE_GAMEPLAY_TAG(Event_Monster_ComboStep_RadialVolleyFinished, "Event.Monster.ComboStep.RadialVolleyFinished");
	UE_DEFINE_GAMEPLAY_TAG(Event_Monster_ComboStep_RadialBurstCascadeFinished, "Event.Monster.ComboStep.RadialBurstCascadeFinished");

	UE_DEFINE_GAMEPLAY_TAG(State_Monster_Dead, "State.Monster.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_HitReact, "State.Monster.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_Attacking, "State.Monster.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_TargetedAttacking, "State.Monster.TargetedAttacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_Charging, "State.Monster.Charging");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_Aiming, "State.Monster.Aiming");
	UE_DEFINE_GAMEPLAY_TAG(State_Monster_AimLocked, "State.Monster.AimLocked");

	UE_DEFINE_GAMEPLAY_TAG(Debuff_Movement_Slow_EliteB, "Debuff.Movement.Slow.EliteB");
}
