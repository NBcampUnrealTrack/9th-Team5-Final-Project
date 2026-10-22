#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ASHealthAttributeSet.generated.h"

/**
 * 체력과, 피해를 받는 쪽의 처리를 담는 어트리뷰트. 능력 시스템을 가진 전원이 갖는다.
 * 피해 전달용 어트리뷰트(IncomingDamage)와 체력 0 처리는 피해 경로를 만들 때 여기에 들어온다.
 *
 * 어트리뷰트 세트는 셋으로 나뉜다. 대상마다 필요한 값이 다르고,
 * 피해를 받는 쪽과 주는 쪽의 처리를 섞지 않기 위해서다.
 *
 *   체력     UASHealthAttributeSet  체력, 피해 전달     전원. 피해를 받는 쪽
 *   전투     UASCombatAttributeSet  공격력, 이동 속도   전원. 피해를 주는 쪽이 읽는 값
 *   플레이어 UASPlayerAttributeSet  스태미나, 마나      플레이어만
 *
 * 몬스터는 체력 + 전투만 받는다. 스태미나나 마나가 없는 상태가 된다.
 * 한 ASC 에 세트를 여러 개 붙일 수 있고, ASC 와 같은 액터의 기본 서브오브젝트로
 * 만들어 두면 ASC 가 초기화될 때 알아서 찾아 등록한다.
 *
 * 전부 모든 접속자에게 복제한다. 남의 체력바를 그려야 하기 때문이다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASHealthAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UASHealthAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 값이 실제로 바뀌기 직전에 불린다. 여기서 범위를 강제한다. */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	//GameplayEffect 실행 직전(서버 전용). 받는 피해에 DamageReduction을 적용한다.
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;

	/** GameplayEffect 실행 직후(서버 전용). 즉발 효과가 바꾼 기본값의 범위를 강제한다. */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ── 체력 ──────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UASHealthAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UASHealthAttributeSet, MaxHealth)

	//받는 피해 감소율 (0 = 감소 없음, 0.2 = 20% 감소, 최대 0.9). GE에서 Add로 올리면 합산
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageReduction, Category = "Attributes|Defense")
	FGameplayAttributeData DamageReduction;
	ATTRIBUTE_ACCESSORS_BASIC(UASHealthAttributeSet, DamageReduction)

protected:
	// OnRep 은 GAMEPLAYATTRIBUTE_REPNOTIFY 를 불러야 예측 보정이 올바르게 동작한다.

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_DamageReduction(const FGameplayAttributeData& OldValue);
};
