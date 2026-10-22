#pragma once

#include "CoreMinimal.h"
#include "Skills/ASSkillAbility.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "ASSkillDash.generated.h"

class UAbilityTask_ApplyRootMotionMoveToForce;

UCLASS()
class APOCALYPSESURVIVAL_API UASSkillDash : public UASSkillAbility
{
	GENERATED_BODY()
	
public:
	UASSkillDash();
	
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility,
		bool bWasCancelled) override;
	
protected:
	virtual void OnSkillEffect_Implementation() override;
	virtual void ApplySkillStats(const FASSkillStatsRow& Stats) override;
	virtual FASSkillIndicatorSettings GetIndicatorSettings_Implementation() const override;

	//End 섹션 재생이 끝나면 스킬 종료
	virtual void OnMontageCompleted() override;
	
	//돌진 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Dash", meta=(ClampMin="0.0", Units="cm"))
	float DashDistance = 600.0f;
	
	//돌진에 걸리는 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Dash", meta=(ClampMin="0.0", Units="s"))
	float DashDuration = 0.25f;
	
	//충돌 판정 반경
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Dash", meta=(ClampMin="0.0", Units="cm"))
	float DashHitRadius = 60.0f;
	
	//무적 효과 GameplayEffect 추가
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Dash")
	TSubclassOf<UGameplayEffect> PostDashEffect;
	
	//사용 후 무적시간 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Dash", meta=(ClampMin="0.0", Units="s"))
	float PostDashDuration = 1.0f;

	//돌진이 멈추면 이 몽타주 섹션(착지 동작)으로 점프 (섹션이 없으면 바로 종료)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Dash|Animation")
	FName DashEndSection = TEXT("End");
private:
	//돌진 도착 또는 시간 초과
	UFUNCTION()
	void OnDashFinished();
	
	//돌진 중 일정 간격으로 충돌 확인
	void CheckDashHit();
	
	//돌진 정지 후 스킬 종료
	void StopDash();
	
	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionMoveToForce> DashTask;
	
	FTimerHandle DashHitTimerHandle;
	
	FVector LastSweepLocation = FVector::ZeroVector;
	
	bool bDashStopped = false;

	//End 섹션 재생 중 (끝나면 스킬 종료)
	bool bPlayingEndSection = false;

	//End 섹션으로 점프 성공하면 true
	bool TryPlayEndSection();

	//돌진·착지 동안 이동 방향으로 몸이 돌지 않게 잠금 (우클릭 이동 명령 무시), 스킬 끝나면 복구
	void LockRotation();
	void UnlockRotation();

	bool bRotationLocked = false;
	bool bSavedOrientRotationToMovement = false;
	bool bSavedUseControllerDesiredRotation = false;
	
	FASCombatTargetHitSet DashHitTargets;
};
