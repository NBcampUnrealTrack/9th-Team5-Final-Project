#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ASAttributeSet.generated.h"

/**
 * 전투에 참여하는 모든 캐릭터(플레이어·적)가 공유하는 어트리뷰트.
 *
 * 현재값(체력·스태미나·마나)은 항상 [0, 최대치] 안으로 강제된다.
 *
 * 전부 모든 접속자에게 복제한다. 남의 체력바를 그려야 하고, 이동 속도는 각자의
 * 화면에서 다른 캐릭터를 움직일 때도 필요하기 때문이다.
 */
UCLASS()
class APOCALYPSESURVIAL_API UASAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UASAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 값이 실제로 바뀌기 직전에 불린다. 여기서 범위를 강제한다. */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** GameplayEffect 실행 직후(서버 전용). 즉발 효과가 바꾼 기본값의 범위를 강제한다. */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ── 체력 ──────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UASAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UASAttributeSet, MaxHealth)

	// ── 스태미나 ──────────────────────────────────────────

	/** 회피, 근접 공격, 물리 스킬이 소모한다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes|Vital")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(UASAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Attributes|Vital")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS_BASIC(UASAttributeSet, MaxStamina)

	// ── 마나 ──────────────────────────────────────────────

	/** 이능·마법 스킬이 소모한다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "Attributes|Vital")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS_BASIC(UASAttributeSet, Mana)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "Attributes|Vital")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS_BASIC(UASAttributeSet, MaxMana)

	// ── 전투 ──────────────────────────────────────────────

	/** 지상 이동 속도(cm/s). 캐릭터가 이 값을 자기 이동 컴포넌트에 그대로 옮겨 쓴다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "Attributes|Combat")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS_BASIC(UASAttributeSet, MoveSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Attributes|Combat")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS_BASIC(UASAttributeSet, AttackPower)

protected:
	// OnRep 은 GAMEPLAYATTRIBUTE_REPNOTIFY 를 불러야 예측 보정이 올바르게 동작한다.

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

private:
	/** 최대치가 있는 어트리뷰트의 현재값을 [0, 최대치] 안으로 맞춘다. */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
