#pragma once

#include "NativeGameplayTags.h"

/** 캐릭터 어빌리티가 쓰는 상태·쿨타임 태그. */
namespace ASGameplayTags
{
	/** 회피 중. 어빌리티가 실행되는 동안 ASC 에 붙는다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);

	/** 회피 쿨타임. 쿨타임 효과(GE_Dodge_Cooldown)가 이 태그를 붙이고, 있는 동안 회피가 막힌다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dodge);
}
