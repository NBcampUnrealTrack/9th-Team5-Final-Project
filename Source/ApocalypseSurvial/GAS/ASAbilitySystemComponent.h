#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ASAbilitySystemComponent.generated.h"

/**
 * 프로젝트 공용 어빌리티 시스템 컴포넌트(ASC).
 *
 * 플레이어의 ASC 는 캐릭터가 아니라 AASPlayerState 에 붙는다. 캐릭터가 죽어 다시
 * 생성돼도 어트리뷰트와 부여된 어빌리티가 유지돼야 하기 때문이다.
 *
 * 지금은 엔진 클래스와 동작이 같다. 따로 클래스를 둔 이유는 입력 슬롯 연결이나
 * 어빌리티 묶음 부여처럼 프로젝트 전체에 걸친 규칙이 생겼을 때 넣을 자리를
 * 한 곳으로 정해 두기 위해서다.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIAL_API UASAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UASAbilitySystemComponent();
};
