#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "ASMonsterTargetedAttackAbility.generated.h"

/** 활성화 시점의 현재 타깃 하나를 잠그고 직접 피해를 주는 몬스터 기본 공격. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterTargetedAttackAbility : public UASMonsterComboStepAbility
{
	GENERATED_BODY()

public:
	UASMonsterTargetedAttackAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void OnSkillEffect_Implementation() override;
	virtual FVector GetAimDirection_Implementation() const override;

private:
	/** 활성화 순간에 확보한 대상. 이후 TargetingComponent가 다른 타깃으로 바뀌어도 바꾸지 않는다. */
	TWeakObjectPtr<AActor> LockedTarget;
};
