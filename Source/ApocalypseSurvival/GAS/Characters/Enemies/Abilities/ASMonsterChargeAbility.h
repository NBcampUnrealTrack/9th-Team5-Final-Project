#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterPatternAbility.h"
#include "ASMonsterChargeAbility.generated.h"

class AASMonsterAttackIndicatorActor;

/**
 * 타깃 방향을 시전 순간에 고정하고, 지정 거리만큼 이동하며 경로의 적대 대상에게 한 번씩 피해를 준다.
 * 선딜과 인디케이터는 다음 단계에서 이 Ability가 유지되는 동안 추가한다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterChargeAbility : public UASMonsterPatternAbility
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge", meta = (ClampMin = "1.0", Units = "cm"))
	float ChargeDistance = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Charge", meta = (ClampMin = "0.01", Units = "s"))
	float ChargeDuration = 0.5f;

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

private:
	void BeginCharge();
	void TickCharge();
	void FinishCharge(bool bWasCancelled);
	void ApplyChargeHitDamage();
	void SpawnChargeIndicator();
	void DestroyChargeIndicator();
	void BeginChargePawnCollisionIgnore();
	void EndChargePawnCollisionIgnore();

	FVector ChargeDirection = FVector::ForwardVector;
	float ChargeElapsedTime = 0.0f;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	TWeakObjectPtr<AASMonsterAttackIndicatorActor> ActiveIndicator;
	FTimerHandle WindupTimerHandle;
	FTimerHandle ChargeTimerHandle;
	ECollisionResponse PreviousPawnCollisionResponse = ECR_Block;
	bool bIsIgnoringPawnCollision = false;
};
