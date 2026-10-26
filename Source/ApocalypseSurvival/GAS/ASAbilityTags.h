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
	
	/**
	 * 쓰러짐 (멀티 전용). 체력이 0이 되면 사망 대신 붙고, 파티원이 부활시키거나 시간이 다 되면 뗀다.
	 * 모든 머신에 복제되므로 이동 정지와 쓰러진연출은 이 태그만 보고 각자 처리한다.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Downed);

	/** 피해 무적. 지속형 GE가 부여하며, 피해 GE는 이 태그가 있으면 적용되지 않는다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);

	/** 플레이어가 피격 경직 또는 넉다운으로 조작을 잠시 잃은 상태. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Staggered);

	/** State.Staggered 중 넉다운 연출을 구분하는 하위 상태. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_KnockedDown);

	/** 피격 제어만 무시하는 슈퍼아머. 피해와 피격 표현은 그대로 적용된다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SuperArmor);

	/** 실제 플레이어 경직 직후 연속 경직만 막는 짧은 보호 상태. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_StaggerProtected);

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

	/** 피해 GameplayEffect 식별용 에셋 태그. ASC에 부여되는 상태 태그가 아니다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Damage);

	/** 무적 GameplayEffect 식별용 에셋 태그. 리스폰 때 남은 무적 효과를 정리하는 데 쓴다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Invulnerable);
}
