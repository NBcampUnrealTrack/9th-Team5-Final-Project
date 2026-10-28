#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ASMonsterSlowGameplayEffect.generated.h"

/**
 * 엘리트 B의 둔화 장판이 적용하는 무한 지속 GameplayEffect.
 * 장판 Actor가 대상별 Handle을 보관하고 마지막 장판에서 이탈할 때 제거한다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMonsterSlowGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UASMonsterSlowGameplayEffect();
};
