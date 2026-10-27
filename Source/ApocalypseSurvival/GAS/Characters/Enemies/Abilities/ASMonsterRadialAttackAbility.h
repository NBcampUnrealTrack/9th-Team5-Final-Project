#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "ASMonsterRadialAttackAbility.generated.h"

class AASMonsterAttackIndicatorActor;

/** 몬스터 중심에서 한 번 피해를 주는 360도 근접 범위 공격. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterRadialAttackAbility : public UASMonsterComboStepAbility
{
	GENERATED_BODY()

public:
	UASMonsterRadialAttackAbility();

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
	virtual void OnSkillEffect_Implementation() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial", meta = (ClampMin = "1.0", Units = "cm"))
	float Radius = 320.0f;

	/** 원형 위험 범위 표시 Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Telegraph")
	TSubclassOf<AASMonsterAttackIndicatorActor> IndicatorClass;

private:
	void BeginRadialAttack();
	void DestroyIndicator();

	TWeakObjectPtr<AASMonsterAttackIndicatorActor> ActiveIndicator;
	FTimerHandle WindupTimerHandle;
};
