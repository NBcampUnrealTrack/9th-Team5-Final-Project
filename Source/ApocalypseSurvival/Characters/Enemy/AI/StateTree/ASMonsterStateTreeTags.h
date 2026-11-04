#pragma once

#include "NativeGameplayTags.h"

/** StateTree Asset 구성과 Linked Asset override에 쓰는 태그다. ASC 상태 태그와 분리한다. */
namespace ASStateTreeTags
{
	/** ST_Monster_Main의 Combat Linked Asset 슬롯. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Combat);
}
