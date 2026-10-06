#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AttributeSet.h"
#include "ASSkillAbility.generated.h"

class ACharacter;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASSkillAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASSkillAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility,
		bool bWasCancelled) override;

protected:
	/** 스킬 효과. 타격 타이밍마다 호출 (기본: 앞쪽 범위 데미지) */
	UFUNCTION(BlueprintNativeEvent, Category = "AS|Skill")
	void OnSkillEffect();

	/** BaseDamage + 공격력 × AttackPowerScale */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "AS|Skill|Damage")
	float CalculateDamage() const;

	/** 스킬을 쓴 캐릭터 (Avatar) */
	UFUNCTION(BlueprintPure, Category = "AS|Skill")
	ACharacter* GetOwnerCharacter() const;

	/** 범위 안의 공격 대상 */
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Target")
	TArray<AActor*> FindTargetsInRadius(const FVector& Center, float Radius) const;

	/** 데미지 적용 (서버 전용) */
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Damage")
	void ApplySkillDamage(AActor* Target);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Animation")
	TObjectPtr<UAnimMontage> AnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	/** 타격 타이밍 이벤트 (비우면 시작 즉시 발동) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Animation", meta = (Categories = "Event"))
	FGameplayTag HitEventTag;

	/** Health Add, SetByCaller(Data.Damage) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage", meta = (ClampMin = "0.0"))
	float AttackPowerScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage")
	FGameplayAttribute AttackPowerAttribute;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Target", meta = (ClampMin = "0.0", Units = "cm"))
	float EffectRadius = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Target", meta = (Units = "cm"))
	float EffectForwardOffset = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Target")
	bool bCanHitPlayers = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Debug")
	bool bDrawDebug = false;

	UFUNCTION()
	virtual void OnMontageCompleted();

	UFUNCTION()
	virtual void OnMontageInterrupted();

private:
	UFUNCTION()
	void HandleHitEvent(FGameplayEventData Payload);

	void FinishSkill(bool bWasCancelled);

	// AnimNotify 누락 경고용
	bool bWaitingForHitEvent = false;
	bool bHitEventReceived = false;
};
