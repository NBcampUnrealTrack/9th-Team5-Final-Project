#pragma once

#include "NativeGameplayTags.h"

/** 일반 몬스터가 사용하는 Ability/State 태그. */
namespace ASGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_Ranged);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_Targeted);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_Charge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_Slam);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_Combo);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_HazardSequence);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_RadialVolley);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_RadialBurstCascade);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_Radial);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_SlowField);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_SniperBeam);

	/** Combo GA가 각 단계의 정상 종료를 구분해 기다리는 이벤트다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Monster_ComboStep_ChargeFinished);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Monster_ComboStep_SlamFinished);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Monster_ComboStep_TargetedFinished);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Monster_ComboStep_RadialFinished);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Monster_ComboStep_HazardSequenceFinished);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Monster_ComboStep_RadialVolleyFinished);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Monster_ComboStep_RadialBurstCascadeFinished);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_Dead);
	/** 피격 연출 중. 이동 및 공격을 막는 짧은 경직 상태다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_HitReact);
	/** 패턴 Ability가 실행 중이다. StateTree와 AI 이동 요청을 대기시키는 공용 상태다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_Attacking);
	/** 타깃을 잠근 단일 대상 공격의 선딜·실행 중이다. 시각 경고에 사용한다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_TargetedAttacking);
	/** 돌진 Ability가 직접 캐릭터 이동을 제어 중이다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_Charging);
	/** 저격 빔이 현재 타깃을 추적 조준 중이다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_Aiming);
	/** 저격 빔의 방향이 고정되어 회피 가능한 마지막 선딜 상태다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_AimLocked);

	/** 엘리트 B의 장판이 적용한 이동속도 감소 효과 식별 태그다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Debuff_Movement_Slow_EliteB);
}
