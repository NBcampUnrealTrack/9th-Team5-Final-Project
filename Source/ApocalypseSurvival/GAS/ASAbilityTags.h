#pragma once

#include "NativeGameplayTags.h"

/** 캐릭터 어빌리티가 쓰는 상태·쿨타임 태그. */
namespace ASGameplayTags
{
	/** 회피 중. 어빌리티가 실행되는 동안 ASC 에 붙는다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);

	/** 회피 쿨타임. 쿨타임 효과(GE_Dodge_Cooldown)가 이 태그를 붙이고, 있는 동안 회피가 막힌다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dodge);

	/**
	 * 사망. 서버가 체력 0 을 확인하면 붙이고 리스폰 때 뗀다.
	 * 모든 머신에 복제되므로 입력 차단과 사망 연출은 이 태그만 보고 각자 처리한다.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	/** 체력이 0 이 되었을 때 ASC 에 보내는 이벤트. 사망 어빌리티가 이 태그로 켜진다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Death);

	/** 이 태그가 붙은 어빌리티는 죽어도 취소되지 않는다. 사망 어빌리티 자신이 대표적이다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Behavior_SurvivesDeath);

	/** 스태미나 소비 직후 회복을 잠시 막는 효과의 식별 태그. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_StaminaRegen_Delay);

	/** 스태미나 회복의 첫 번째 느린 단계 식별 태그. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_StaminaRegen_Slow);

	/** 스태미나 회복의 두 번째 빠른 단계 식별 태그. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_StaminaRegen_Fast);
}
