#pragma once

#include "CoreMinimal.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "ASMonsterChargeAbility.generated.h"

class AASMonsterAttackIndicatorActor;

/**
 * 타깃 방향을 시전 순간에 고정하고, 지정 거리만큼 이동하며 경로의 적대 대상에게 한 번씩 피해를 준다.
 * 단독 패턴으로 끝낼 수 있으며, Combo GA가 기다리는 경우 정상 완료 이벤트로 다음 단계를 이어 준다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterChargeAbility : public UASMonsterComboStepAbility
{
	GENERATED_BODY()

public:
	UASMonsterChargeAbility();

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
	virtual FVector GetAimDirection_Implementation() const override;

	/** 돌진 이동이 끝난 뒤의 후속 동작. 기본 돌진은 즉시 Ability를 종료한다. */
	virtual void OnChargeFinished();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge", meta = (ClampMin = "1.0", Units = "cm"))
	float ChargeDistance = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge", meta = (ClampMin = "0.01", Units = "s"))
	float ChargeDuration = 0.5f;

	/** true면 시전 순간 타깃과의 거리를 기준으로 실제 돌진 길이를 고정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge|Targeting")
	bool bUseTargetDistance = false;

	/** 타깃 거리 추적 시 타깃 직전에서 멈추기 위해 남겨 둘 거리다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge|Targeting", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bUseTargetDistance"))
	float TargetStopDistance = 150.0f;

	/** 타깃 거리 추적 시에도 확보할 최소 이동 거리다. 0이면 가까운 타깃 앞에서 제자리 슬램이 가능하다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge|Targeting", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bUseTargetDistance"))
	float MinTargetChargeDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge", meta = (ClampMin = "1.0", Units = "cm"))
	float HitRadius = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge", meta = (ClampMin = "0.005", Units = "s"))
	float MovementTickInterval = 0.02f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge")
	bool bStopOnBlockingHit = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Telegraph")
	TSubclassOf<AASMonsterAttackIndicatorActor> IndicatorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Telegraph", meta = (ClampMin = "1.0", Units = "cm"))
	float IndicatorWidth = 240.0f;

	/** 자식 Ability가 돌진 피해를 별도 수치·효과로 바꿀 때 재정의한다. */
	virtual void ApplyChargeHitDamage();

	/** 돌진 타이머와 Pawn 충돌 무시를 정리하되 Ability는 유지한다. */
	void EndChargeMovement();

	void FinishCharge(bool bWasCancelled);

	FVector ChargeDirection = FVector::ForwardVector;
	FASCombatTargetHitSet HitTargets;

	void BeginChargePawnCollisionIgnore();
	void EndChargePawnCollisionIgnore();

private:
	void BeginCharge();
	void TickCharge();
	void SpawnChargeIndicator();
	void DestroyChargeIndicator();

	float ChargeElapsedTime = 0.0f;
	float ResolvedChargeDistance = 0.0f;
	TWeakObjectPtr<AASMonsterAttackIndicatorActor> ActiveIndicator;
	FTimerHandle WindupTimerHandle;
	FTimerHandle ChargeTimerHandle;
	ECollisionResponse PreviousPawnCollisionResponse = ECR_Block;
	bool bIsIgnoringPawnCollision = false;
};
