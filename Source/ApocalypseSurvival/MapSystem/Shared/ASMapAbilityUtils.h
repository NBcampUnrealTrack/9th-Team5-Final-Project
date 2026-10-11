#pragma once

#include "CoreMinimal.h"

class AActor;
class UAbilitySystemComponent;

/** 시야와 구역 효과가 공유하는 어빌리티 시스템 조회 헬퍼. */
namespace ASMapAbilityUtils
{
	/** 플레이어는 보통 PlayerState에 어빌리티 시스템을 두고, 몬스터는 직접 소유한다. */
	APOCALYPSESURVIVAL_API UAbilitySystemComponent* FindAbilitySystemComponent(const AActor* Actor);
}
