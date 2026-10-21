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

	//강태오 레드라인 버프 중 (나중에 적중 시 충격파 등 조건에 사용)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Taeo_Redline);
}
