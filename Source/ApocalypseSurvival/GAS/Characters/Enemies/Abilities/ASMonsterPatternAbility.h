#pragma once

#include "CoreMinimal.h"
#include "Skills/ASSkillAbility.h"
#include "ASMonsterPatternAbility.generated.h"

/**
 * 엘리트·보스의 선딜 패턴 전용 Ability 기반 클래스.
 * 공용 UASSkillAbility의 동작은 건드리지 않고, 패턴의 선딜/실행/정리 생명주기만 별도로 제공한다.
 */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterPatternAbility : public UASSkillAbility
{
	GENERATED_BODY()

public:
	UASMonsterPatternAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float WindupDuration = 0.0f;

	/** 선딜 시작 시 인디케이터 생성·방향 고정 등을 처리한다. */
	virtual void BeginPatternWindup();

	/** true면 실행 직후 Ability를 종료한다. 돌진처럼 지속 실행이면 false를 반환한다. */
	virtual bool ExecutePatternEffect();

	/** 취소·사망을 포함해 패턴이 끝날 때 인디케이터 등을 정리한다. */
	virtual void EndPattern(bool bWasCancelled);

	void FinishPattern(bool bWasCancelled);

private:
	void BeginPatternEffect();
	FTimerHandle WindupTimerHandle;
};
