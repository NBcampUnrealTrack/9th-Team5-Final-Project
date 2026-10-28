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
 * 입력 태그로 어빌리티를 발동하는 규칙이 여기에 있다. 컨트롤러는 "어떤 태그가
 * 눌렸다"만 넘기고, 어느 어빌리티가 반응할지는 ASC 가 태그를 보고 정한다.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UASAbilitySystemComponent();
	virtual bool AreAbilityTagsBlocked(const FGameplayTagContainer& Tags) const override;

	/**
	 * 입력 태그가 눌렸다. 태그가 맞는 어빌리티를 발동하거나, 이미 실행 중이면
	 * "눌림"을 알린다(차징처럼 누르고 있는 동안을 보는 어빌리티용).
	 */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	/** 입력 태그가 떼였다. 실행 중인 어빌리티에 "뗌"을 알린다. */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/** 주인이 죽어 있는가. State.Dead 태그가 있으면 죽은 것이다. */
	bool IsDead() const;

private:
	/**
	 * 이 어빌리티가 그 입력 태그에 반응하는가.
	 * 부여할 때 붙인 태그(어빌리티 세트 방식)와 어빌리티 자체의 에셋 태그 둘 다 본다.
	 * 무기 어빌리티처럼 다른 코드가 부여하는 것은 에셋 태그에 적어 두면 된다.
	 */
	static bool DoesSpecMatchInputTag(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag);
};
