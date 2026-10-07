#pragma once

#include "NativeGameplayTags.h"

/**
 * 입력 태그. "어떤 키가 눌렸는가"를 어빌리티에 전달하는 이름표다.
 *
 * 키와 어빌리티를 직접 묶지 않고 태그를 사이에 둔다. 그래야 무기를 바꾸면
 * 좌클릭이 그 무기의 공격으로 바뀌고, 스킬 담당은 어빌리티에 태그만 달면
 * QWER 에 연결된다. 키 → 태그는 UASInputConfig 가, 태그 → 어빌리티는
 * 어빌리티의 태그(에셋 태그 또는 부여할 때 붙인 태그)가 정한다.
 */
namespace ASGameplayTags
{
	/** 장착한 무기의 일반 공격 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Attack_Primary);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Dodge);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_W);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_R);
}
