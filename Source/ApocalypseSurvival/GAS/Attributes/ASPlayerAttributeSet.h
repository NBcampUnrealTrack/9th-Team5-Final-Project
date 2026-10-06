#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ASPlayerAttributeSet.generated.h"

/**
 * 플레이어만 갖는 어트리뷰트. 행동 자원이다.
 *
 * 몬스터에게는 붙이지 않는다. 몬스터의 공격은 쿨타임만으로 제한하고 자원을 쓰지 않는다.
 * 세트를 나눈 이유는 UASHealthAttributeSet 머리말에 있다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASPlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UASPlayerAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 값이 실제로 바뀌기 직전에 불린다. 여기서 범위를 강제한다. */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** GameplayEffect 실행 직후(서버 전용). 즉발 효과가 바꾼 기본값의 범위를 강제한다. */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ── 스태미나 ──────────────────────────────────────────

	/** 회피, 근접 공격, 물리 스킬이 소모한다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes|Resource")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(UASPlayerAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Attributes|Resource")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS_BASIC(UASPlayerAttributeSet, MaxStamina)

	// ── 마나 ──────────────────────────────────────────────

	/** 이능·마법 스킬이 소모한다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "Attributes|Resource")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS_BASIC(UASPlayerAttributeSet, Mana)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "Attributes|Resource")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS_BASIC(UASPlayerAttributeSet, MaxMana)

protected:
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);

private:
	/** 최대치가 있는 어트리뷰트의 현재값을 [0, 최대치] 안으로 맞춘다. */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
