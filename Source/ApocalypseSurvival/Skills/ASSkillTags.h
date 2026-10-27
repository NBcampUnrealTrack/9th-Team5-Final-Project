#pragma once

#include "NativeGameplayTags.h"

//스킬 네이티브 태그
namespace ASGameplayTags
{
	//타격 타이밍
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Hit);

	//데미지 SetByCaller 키
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	//마나 소모 SetByCaller 키 (GE_SkillCost, 음수로 넣음)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_ManaCost);

	//스태미나 소모 SetByCaller 키 (GE_SkillCost, 음수로 넣음. 일반공격 비용과 같은 태그)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_StaminaCost);

	//스킬 시전 중
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Casting);

	//일반 시전 조준 중 (좌클릭이 기본 공격 대신 조준 확정이 됨, 로컬 전용)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Targeting);
	
	//스킬 쿨타임 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Slash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Throw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_DoT);
	
	//공용 스킬 쿨타임 태크
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Shared);

	//강태오 스킬 쿨타임 태그 (DA_Skill_*의 Cooldown Tag에 사용)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_BreakRush);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Overclock);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Redline);

	//재이 스킬 쿨타임 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_ForceRepulse);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_CatchSling);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_SpreadOrb);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_ZeroGravity);

	//강태오 레드라인 버프 중 (나중에 적중 시 충격파 등 조건에 사용)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Taeo_Redline);

	//강태오 오버클록 버프 중 (GE_Taeo_Overclock이 붙임, 적중 시 스태미나 회복 조건)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Taeo_Overclock);

	//강태오 데드 앵글 발동 중 (GE_Taeo_DeadAngle이 붙임, 표시·조건용 - 감소량은 DamageReduction 어트리뷰트)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Taeo_DeadAngle);

	//적중 이벤트 (피해를 준 순간 공격자 ASC로 전송, 전투 서브시스템에서 보낼 예정)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Dealt);

	//공격 종류: 기본 공격 (적중 이벤트 InstigatorTags에 담아 구분)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack_Basic);
}
