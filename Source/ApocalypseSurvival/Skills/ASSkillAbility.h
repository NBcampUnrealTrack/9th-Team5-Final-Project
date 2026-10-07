#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AttributeSet.h"
#include "ASSkillAbility.generated.h"

class ACharacter;
class UAnimMontage;
class UGameplayEffect;

/**
 * Base for Skills
 */
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

	//공격 대상이 될 수 있는지 (자신·아군 제외, ASC 보유)
	static bool IsValidSkillTarget(const AActor* Target, const AActor* Instigator, bool bCanHitPlayers);

	//원 범위 안의 공격 대상
	static TArray<AActor*> FindSkillTargets(const UWorld* World, const FVector& Center, float Radius, const AActor* Instigator, bool bCanHitPlayers);

	//데미지 스펙을 대상에게 적용 (서버에서 호출)
	static void ApplyDamageSpecToTarget(const FGameplayEffectSpecHandle& DamageSpec, AActor* Target);

protected:
	//스킬 효과. 타격 타이밍마다 호출
	UFUNCTION(BlueprintNativeEvent, Category = "AS|Skill")
	void OnSkillEffect();

	//BaseDamage + 공격력 × AttackPowerScale
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "AS|Skill|Damage")
	float CalculateDamage() const;

	//조준 방향 (기본: 캐릭터 정면)
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "AS|Skill")
	FVector GetAimDirection() const;

	//스킬을 쓴 캐릭터 (Avatar)
	UFUNCTION(BlueprintPure, Category = "AS|Skill")
	ACharacter* GetOwnerCharacter() const;

	//범위 안의 공격 대상
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Target")
	TArray<AActor*> FindTargetsInRadius(const FVector& Center, float Radius) const;

	//데미지 스펙 생성 (투사체·범위 액터에 넘길 때 사용)
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Damage")
	FGameplayEffectSpecHandle MakeDamageSpec() const;

	//데미지 적용 (서버 전용)
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Damage")
	void ApplySkillDamage(AActor* Target);

	//서버인지
	bool HasSkillAuthority() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Animation")
	TObjectPtr<UAnimMontage> AnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	//타격 타이밍 이벤트 (비우면 시작 즉시 발동)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Animation", meta = (Categories = "Event"))
	FGameplayTag HitEventTag;

	//Health Add, SetByCaller(Data.Damage)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage", meta = (ClampMin = "0.0"))
	float AttackPowerScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage")
	FGameplayAttribute AttackPowerAttribute;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Target")
	bool bCanHitPlayers = false;

	//[임시] 마우스 커서 방향으로 조준 (로컬 플레이어만, 멀티 미지원)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Aim")
	bool bAimAtCursor = true;

	//시전 시 캐릭터를 조준 방향으로 회전
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Aim")
	bool bFaceAimDirection = true;

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

	//시전 순간의 조준 방향 계산
	FVector ComputeAimDirection() const;

	//시전 순간에 저장한 조준 방향 (타격 때 사용)
	FVector CachedAimDirection = FVector::ForwardVector;
	bool bHasCachedAim = false;

	//AnimNotify 누락 경고용
	bool bWaitingForHitEvent = false;
	bool bHitEventReceived = false;
};
