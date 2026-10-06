#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ASCommonAttributeSet.generated.h"

/**
 * 능력 시스템을 가진 모든 대상(플레이어·엘리트·보스)이 공통으로 갖는 어트리뷰트.
 *
 * 어트리뷰트 세트는 셋으로 나뉜다. 대상마다 필요한 값이 다르기 때문이다.
 *
 *   공용     UASCommonAttributeSet  체력              전원
 *   전투     UASCombatAttributeSet  공격력, 이동 속도   전원. 피해 계산이 읽는 값
 *   플레이어 UASPlayerAttributeSet  스태미나, 마나      플레이어만
 *
 * 몬스터는 공용 + 전투만 받는다. 스태미나나 마나가 없는 상태가 된다.
 * 한 ASC 에 세트를 여러 개 붙일 수 있고, ASC 와 같은 액터의 기본 서브오브젝트로
 * 만들어 두면 ASC 가 초기화될 때 알아서 찾아 등록한다.
 *
 * 전부 모든 접속자에게 복제한다. 남의 체력바를 그려야 하기 때문이다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASCommonAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UASCommonAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 값이 실제로 바뀌기 직전에 불린다. 여기서 범위를 강제한다. */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** GameplayEffect 실행 직후(서버 전용). 즉발 효과가 바꾼 기본값의 범위를 강제한다. */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ── 체력 ──────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UASCommonAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UASCommonAttributeSet, MaxHealth)

protected:
	// OnRep 은 GAMEPLAYATTRIBUTE_REPNOTIFY 를 불러야 예측 보정이 올바르게 동작한다.

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
