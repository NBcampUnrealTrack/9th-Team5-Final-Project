#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "ASMonsterSlamKnockbackAbility.generated.h"

class AASMonsterAttackIndicatorActor;

/** 원형 예고 뒤 범위 피해와 방사형 넉백을 주는 재사용 가능한 몬스터 패턴 Ability. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterSlamKnockbackAbility : public UASMonsterComboStepAbility
{
	GENERATED_BODY()

public:
	UASMonsterSlamKnockbackAbility();

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Slam", meta = (ClampMin = "0.0", Units = "s"))
	float SlamWindupDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Slam", meta = (ClampMin = "1.0", Units = "cm"))
	float SlamRadius = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Slam", meta = (ClampMin = "0.0"))
	float DamageScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Knockback", meta = (ClampMin = "0.0", Units = "cm"))
	float KnockbackDistance = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Knockback", meta = (ClampMin = "0.05", Units = "s"))
	float KnockbackDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Knockback", meta = (ClampMin = "0.0", Units = "cm"))
	float KnockbackHeight = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Telegraph")
	TSubclassOf<AASMonsterAttackIndicatorActor> IndicatorClass;

private:
	void ExecuteSlam();
	void SpawnIndicator();
	void DestroyIndicator();
	void ApplyKnockback(AActor* Target, const FVector& SlamOrigin) const;
	FGameplayEffectSpecHandle MakeScaledDamageSpec() const;

	FTimerHandle SlamWindupTimerHandle;
	TWeakObjectPtr<AASMonsterAttackIndicatorActor> ActiveIndicator;
};
