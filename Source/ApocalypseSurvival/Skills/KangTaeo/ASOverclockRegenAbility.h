#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "ASOverclockRegenAbility.generated.h"

class UGameplayEffect;

/*
 * 강태오 오버클록: 5m 이내 적에게 기본 공격 적중 시 스태미나 소량 회복 (최대 0.5초에 1회)
 * 입력 없이 적중 이벤트(Event.Hit.Dealt)로 자동 발동, 오버클록 중(State.Taeo.Overclock)일 때만
 * 부여 방법: GE_Taeo_Overclock의 Grant Gameplay Abilities 컴포넌트 (오버클록 동안만 보유)
 * 전제: 전투 서브시스템이 피해 적용 시 공격자 ASC로 Event.Hit.Dealt 전송
 *       Payload.Target = 맞은 Actor (Mass면 TargetData에 위치), InstigatorTags에 Attack.Basic
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASOverclockRegenAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASOverclockRegenAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	//회복 GE (Instant, Stamina Add)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Taeo|Overclock")
	TSubclassOf<UGameplayEffect> StaminaGainEffect;

	//이 거리 안의 적을 맞혔을 때만 회복
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Taeo|Overclock", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxHitDistance = 500.0f;

	//회복 최소 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Taeo|Overclock", meta = (ClampMin = "0.0", Units = "s"))
	float GainInterval = 0.5f;

	//이 공격 종류일 때만 회복 (비우면 모든 적중)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Taeo|Overclock")
	FGameplayTag RequiredAttackTag;

private:
	//적중 위치 (Actor면 Actor 위치, Mass면 TargetData 위치)
	static bool GetHitLocation(const FGameplayEventData& EventData, FVector& OutLocation);

	//마지막으로 회복한 시간 (간격 제한용)
	double LastGainTime = -1000.0;
};
