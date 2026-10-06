#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ASCombatAttributeSet.generated.h"

/**
 * 전투 계산에 쓰이는 어트리뷰트. 플레이어와 몬스터가 모두 갖는다.
 *
 * 피해 계산 클래스가 공격자 쪽에서 읽는 값을 여기에 모은다. 피해를 받는 쪽의
 * 어트리뷰트(IncomingDamage)는 여기가 아니라 UASHealthAttributeSet 에 둔다.
 * 세트를 나눈 이유는 UASHealthAttributeSet 머리말에 있다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UASCombatAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 값이 실제로 바뀌기 직전에 불린다. 여기서 범위를 강제한다. */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Attributes|Combat")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS_BASIC(UASCombatAttributeSet, AttackPower)

	/** 지상 이동 속도(cm/s). 캐릭터가 이 값을 자기 이동 컴포넌트에 그대로 옮겨 쓴다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "Attributes|Combat")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS_BASIC(UASCombatAttributeSet, MoveSpeed)

protected:
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
};
