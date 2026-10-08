#pragma once

#include "NativeGameplayTags.h"

/** 일반 몬스터가 사용하는 Ability/State 태그. */
namespace ASGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Monster_Attack_Ranged);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_Dead);
	/** 피격 연출 중. 이동 및 공격을 막는 짧은 경직 상태다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_HitReact);
}
