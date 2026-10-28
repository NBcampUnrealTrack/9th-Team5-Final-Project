#pragma once

#include "CoreMinimal.h"
#include "Skills/Melee/ASSkillMelee.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "ASSkillForceRepulse.generated.h"

/*
 * 재이 Q 포스 리펄스: 전방 부채꼴 염동 충격 (신드라 E 느낌)
 * - 파동: WaveDuration 동안 부채꼴 반경을 0 -> 최대로 키우며 판정, 파동이 닿은 순서대로 피해 (Actor, 대상당 1회)
 * - Mass: 전투 서브시스템의 부채꼴 판정이 중복 방지(HitSet)를 지원하지 않아서, 시전 순간 전체 범위로 1회만 피해
 * - 넉백: bApplyKnockback을 켜면 맞은 Actor 캐릭터를 포물선으로 밀어냄 (기본 꺼짐)
 * 1차: 링크 대상 강화, 벽 충돌 추가 피해, 투사체 밀어내기는 미구현
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASSkillForceRepulse : public UASSkillMelee
{
	GENERATED_BODY()

public:
	UASSkillForceRepulse();

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility,
		bool bWasCancelled) override;

protected:
	virtual void OnSkillEffect_Implementation() override;

	//몽타주가 끝나도 파동이 끝날 때까지 기다렸다가 종료
	virtual void OnMontageCompleted() override;

	//파동이 끝까지 퍼지는 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ForceRepulse", meta = (ClampMin = "0.0", Units = "s"))
	float WaveDuration = 0.25f;

	//파동 판정 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ForceRepulse", meta = (ClampMin = "0.01", Units = "s"))
	float WaveTickInterval = 0.03f;

	//true = 맞은 Actor 적을 밀어냄 (팀 의견으로 기본 꺼짐, 피해와 경직만)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ForceRepulse")
	bool bApplyKnockback = false;

	//밀려나는 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ForceRepulse", meta = (EditCondition = "bApplyKnockback", ClampMin = "0.05", Units = "s"))
	float KnockbackDuration = 0.3f;

	//포물선 높이 (0이면 바닥으로 미끄러짐)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ForceRepulse", meta = (EditCondition = "bApplyKnockback", ClampMin = "0.0", Units = "cm"))
	float KnockbackHeight = 80.0f;

	//재이로부터 (범위 끝 + 이 거리)까지 밀려남 - 가까운 적일수록 멀리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ForceRepulse", meta = (EditCondition = "bApplyKnockback", ClampMin = "0.0", Units = "cm"))
	float PushExtraDistance = 150.0f;

	//범위 끝에 있던 적도 최소 이만큼은 밀림
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ForceRepulse", meta = (EditCondition = "bApplyKnockback", ClampMin = "0.0", Units = "cm"))
	float MinPushDistance = 100.0f;

private:
	void BeginWaveDamage();

	//파동 한 틱: 현재 반경까지 판정 + 새로 맞은 대상 넉백 (서버)
	void TickWave();

	void StopWave();

	//파동과 몽타주가 모두 끝나면 스킬 종료
	void TryFinish();

	void ApplyKnockback(AActor* Target) const;

	//Mass 몬스터는 시전 순간 전체 부채꼴로 1회 피해
	void ApplyMassDamageOnce() const;

	FTimerHandle WaveTimerHandle;
	FTimerHandle DeferredWaveTimer;

	//시전 순간 고정한 값
	FVector WaveOrigin = FVector::ZeroVector;
	FVector WaveDirection = FVector::ForwardVector;
	FGameplayEffectSpecHandle WaveDamageSpec;
	float WaveElapsed = 0.0f;

	//이미 맞은 대상 (Actor + Mass, 대상당 1회)
	FASCombatTargetHitSet WaveHitTargets;

	bool bWaveActive = false;
	bool bMontageDone = false;
};
