#pragma once

#include "NativeGameplayTags.h"

//스킬 네이티브 태그
namespace ASGameplayTags
{
	//타격 타이밍
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Hit);

	//데미지 SetByCaller 키
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	//스킬 시전 중
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Casting);
	
	//스킬 쿨타임 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Slash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Throw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_DoT);
}
