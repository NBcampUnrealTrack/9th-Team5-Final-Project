#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ASGameplayAbility_Death.generated.h"

class AASPlayerCharacter;
class UAnimMontage;

/**
 * 사망 어빌리티. Lyra 의 LyraGameplayAbility_Death 를 따른다.
 *
 * 입력이 아니라 GameplayEvent.Death 이벤트로만 켜진다. 캐릭터가 체력 0 을 보고 이벤트를 보내면
 * 서버에서 이 어빌리티가 활성화되고, 본인 클라이언트에도 같이 활성화된다(ServerInitiated).
 *
 * 하는 일은 순서대로 셋이다.
 * 1. 다른 어빌리티를 전부 취소한다. Ability.Behavior.SurvivesDeath 가 붙은 것만 남긴다.
 * 2. 캐릭터의 StartDeath 를 불러 사망 태그를 붙인다. 이동 정지와 연출은 태그를 받은 각 머신이 한다.
 * 3. 사망 몽타주를 재생하고 끝나면 FinishDeath 를 불러 게임모드에 리스폰을 맡긴다.
 *
 * 몽타주는 BP 자식(GA_Death)에서 지정한다. 비어 있으면 MinDeathDuration 뒤에 끝낸다.
 * 연출을 BP 그래프에서 직접 하고 싶으면 bAutoFinishDeath 를 끄고 FinishDeath 를 직접 부른다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASGameplayAbility_Death : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASGameplayAbility_Death();

	/** 사망을 끝낸다. 서버에서는 게임모드에 알리고, 양쪽 모두 어빌리티를 종료한다. */
	UFUNCTION(BlueprintCallable, Category = "Death")
	void FinishDeath();

protected:
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

private:
	UFUNCTION()
	void HandleMontageFinished();

	AASPlayerCharacter* GetPlayerCharacter() const;

	/** 사망 몽타주. 끝나면(또는 끊기면) FinishDeath 로 이어진다. 비어 있으면 MinDeathDuration 뒤에 끝낸다. */
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** 몽타주가 없을 때 쓰러진 모습을 보여 줄 최소 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ClampMin = "0.0", Units = "s"))
	float MinDeathDuration = 1.0f;

	/**
	 * 켜져 있으면 몽타주가 끝날 때 C++ 이 FinishDeath 를 부른다.
	 * BP 그래프에서 연출 순서를 직접 짜려면 끄고 FinishDeath 를 직접 불러야 한다. 안 부르면 리스폰되지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	bool bAutoFinishDeath = true;

	FTimerHandle MinDurationTimerHandle;
};
