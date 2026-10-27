#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterPatternAbility.h"
#include "ASMonsterComboAbility.generated.h"

class UAbilityTask_WaitGameplayEvent;

/** 콤보에서 실행할 재사용 Ability 한 단계의 데이터다. */
USTRUCT(BlueprintType)
struct FASMonsterComboStep
{
	GENERATED_BODY()

	/** 보스에게 부여된 개별 Ability의 고유 공격 태그. Ability.Monster.Attack 같은 상위 태그는 사용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (Categories = "Ability.Monster.Attack"))
	FGameplayTag AbilityTag;

	/** 해당 Ability가 정상 종료 시 전송하는 완료 이벤트 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (Categories = "Event"))
	FGameplayTag CompletionEventTag;

	/** 이전 단계가 끝난 후 다음 Ability를 시작하기 전에 기다리는 시간. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "0.0", Units = "s"))
	float TransitionDelay = 0.0f;

	/** 완료 이벤트가 오지 않으면 콤보를 취소하는 최대 대기 시간. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "0.01", Units = "s"))
	float Timeout = 3.0f;
};

/**
 * TArray에 설정한 임의 개수의 몬스터 Ability를 순서대로 실행하는 상위 콤보 Ability.
 * 개별 공격은 재사용 가능하고, 이 Ability는 순서·전환·타임아웃만 관리한다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterComboAbility : public UASMonsterPatternAbility
{
	GENERATED_BODY()

public:
	UASMonsterComboAbility();

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
	/** 순서대로 실행할 콤보 단계. 2개 이상으로 제한하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Combo", meta = (TitleProperty = "AbilityTag"))
	TArray<FASMonsterComboStep> ComboSteps;

private:
	void StartNextStep();
	void BeginWaitingForStep(const FASMonsterComboStep& Step);
	void StopWaitingForStep();
	void CancelActiveStep();
	void FinishCombo(bool bWasCancelled);

	UFUNCTION()
	void OnStepCompleted(FGameplayEventData EventData);

	void OnStepTimedOut();

	int32 ActiveStepIndex = INDEX_NONE;
	FGameplayTag ActiveStepAbilityTag;
	FTimerHandle StepTimeoutTimerHandle;
	FTimerHandle TransitionTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CompletionEventTask;
};
