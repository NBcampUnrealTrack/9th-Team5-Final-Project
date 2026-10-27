#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "ASMonsterRadialBurstCascadeAbility.generated.h"

class AASMonsterAttackIndicatorActor;

/**
 * 보스 중심에서 고정된 여러 방향으로 원형 폭발 지점이 단계적으로 바깥으로 진행하는 장판 패턴.
 * 각 단계에는 DirectionCount개만 예고·폭발해, 많은 단계에도 동시 장판 수를 제한한다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterRadialBurstCascadeAbility : public UASMonsterComboStepAbility
{
	GENERATED_BODY()

public:
	UASMonsterRadialBurstCascadeAbility();

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Burst", meta = (ClampMin = "1"))
	int32 DirectionCount = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Burst", meta = (ClampMin = "1"))
	int32 BurstStageCount = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Burst", meta = (ClampMin = "0.0", Units = "cm"))
	float InitialBurstRadius = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Burst", meta = (ClampMin = "1.0", Units = "cm"))
	float BurstStepDistance = 280.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Burst", meta = (ClampMin = "1.0", Units = "cm"))
	float BurstRadius = 145.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Burst", meta = (ClampMin = "0.05", Units = "s"))
	float TelegraphDuration = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Burst", meta = (ClampMin = "0.0", Units = "s"))
	float StageDelay = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Burst")
	bool bRandomStartAngle = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Burst", meta = (ClampMin = "0.0"))
	float DamageScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Telegraph")
	TSubclassOf<AASMonsterAttackIndicatorActor> IndicatorClass;

private:
	void BeginCurrentStage();
	void ExplodeCurrentStage();
	void SpawnCurrentStageIndicators();
	void DestroyCurrentStageIndicators();
	void ApplyCurrentStageDamage();
	FGameplayEffectSpecHandle MakeScaledDamageSpec() const;
	FVector GetStageLocation(int32 DirectionIndex) const;
	FVector GetStageDirection(int32 DirectionIndex) const;

	int32 CurrentStageIndex = 0;
	float StartAngleDegrees = 0.0f;
	FVector CascadeOrigin = FVector::ZeroVector;
	TArray<TWeakObjectPtr<AASMonsterAttackIndicatorActor>> ActiveIndicators;
	FTimerHandle TelegraphTimerHandle;
	FTimerHandle NextStageTimerHandle;
};
