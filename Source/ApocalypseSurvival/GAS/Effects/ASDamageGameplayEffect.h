#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ASDamageGameplayEffect.generated.h"

/**
 * 프로젝트의 모든 피해 GameplayEffect가 상속하는 공통 기반 클래스.
 *
 * 피해용 BP GameplayEffect는 반드시 이 클래스를 부모로 사용한다. 대상 태그 조건은
 * UE 5.8의 GameplayEffectComponent 생성 규칙상 BP 에셋에서 구성한다.
 *
 * 필수 BP 설정:
 * - Require Tags to Apply/Continue This Effect: Application/Ongoing Ignore = State.Invulnerable
 * - Tags This Effect Has (Asset Tags): Effect.Damage
 */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASDamageGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

};
