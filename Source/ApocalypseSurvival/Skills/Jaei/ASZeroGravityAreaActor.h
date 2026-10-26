#pragma once

#include "CoreMinimal.h"
#include "Skills/Area/ASSkillAreaActor.h"
#include "ASZeroGravityAreaActor.generated.h"

/*
 * 재이 R 제로 그래비티 존의 장판
 * - 장판 안 Actor 적마다 StunRollInterval 간격으로 최대 StunRollCount번 StunChance 확률 판정 (처음 닿은 순간 1번째, 성공하면 그 적은 끝)
 * - 장판이 끝날 때 중심에서 폭발: 마지막 틱에 장판 안에 있던 수(Mass 포함)만큼 피해·반경 증가
 * - Mass는 스턴 불가 (진강님 상태이상 함수 생기면 추가), 피해는 들어감
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASZeroGravityAreaActor : public AASSkillAreaActor
{
	GENERATED_BODY()

public:
	//FinishSpawning 전에 호출 (UASSkillZeroGravity::ConfigureArea)
	void InitZeroGravity(
		const FGameplayEffectSpecHandle& InStunSpec,
		float InStunChance,
		int32 InStunRollCount,
		float InStunRollInterval,
		const FGameplayEffectSpecHandle& InBurstSpec,
		float InBurstBonusPerTarget,
		float InBurstRadiusPerTarget,
		int32 InMaxBurstTargets,
		EASImpactPreset InBurstImpactPreset);

protected:
	virtual void OnAreaTick(const FASCombatApplyResult& Result, const FVector& Center) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//종료 폭발 GameplayCue (RawMagnitude = 폭발 반경)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity|Cue", meta = (Categories = "GameplayCue"))
	FGameplayTag BurstCueTag;

	//스턴 성공한 대상 위치에 실행할 GameplayCue (RawMagnitude = 0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity|Cue", meta = (Categories = "GameplayCue"))
	FGameplayTag StunCueTag;

	//스턴 성공 시 (서버)
	UFUNCTION(BlueprintImplementableEvent, Category = "AS|Skill|ZeroGravity")
	void BP_OnTargetStunned(AActor* Target);

private:
	//간격·횟수가 남았으면 확률 판정
	void TryStun(AActor* Target);

	//대상별 스턴 판정 기록
	struct FStunRollState
	{
		int32 RollCount = 0;
		double NextRollTime = 0.0;
		bool bStunned = false;
	};

	void DoBurst();

	FGameplayEffectSpecHandle StunSpec;
	float StunChance = 0.3f;
	int32 StunRollCount = 2;
	float StunRollInterval = 2.0f;

	FGameplayEffectSpecHandle BurstSpec;
	float BurstBonusPerTarget = 0.1f;
	float BurstRadiusPerTarget = 0.02f;
	int32 MaxBurstTargets = 20;
	EASImpactPreset BurstImpactPreset = EASImpactPreset::Knockdown;

	TMap<TWeakObjectPtr<AActor>, FStunRollState> StunRolls;

	//마지막 틱에 장판 안에 있던 수 (Actor + Mass)
	int32 LastHitCount = 0;
	bool bBurstDone = false;
};
