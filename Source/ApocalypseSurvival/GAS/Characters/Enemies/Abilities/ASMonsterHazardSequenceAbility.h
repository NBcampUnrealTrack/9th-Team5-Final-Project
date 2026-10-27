#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "ASMonsterHazardSequenceAbility.generated.h"

struct FASCombatTargetHitSet;
class AASMonsterAttackIndicatorActor;

/** 장판 한 묶음의 형태. Cross는 서로 직교하는 두 직사각형으로 구성된다. */
UENUM(BlueprintType)
enum class EASMonsterHazardShape : uint8
{
	Circle,
	Cross
};

/** 보스 중심 주변에서 장판을 배치하는 방식. */
UENUM(BlueprintType)
enum class EASMonsterHazardPlacement : uint8
{
	RandomAnnulus,
	Ring,
	/** 각 플레이어 주변에 Count개씩 무작위 원형 장판을 배치한다. */
	PerPlayerRandomAnnulus,
	/** 각 플레이어의 이동 예측 경로를 가로막도록 Count개씩 배치한다. */
	PerPlayerPredictedPath,
	/** 각 플레이어 주변을 Count개 장판으로 둘러싸되, 탈출할 수 있는 틈을 남긴다. */
	PerPlayerSurround
};

/** 한 번의 예고 후 동시에 폭발하는 장판 묶음. 여러 항목을 배열에 넣어 연쇄 폭발을 만든다. */
USTRUCT(BlueprintType)
struct FASMonsterHazardWave
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard")
	EASMonsterHazardShape Shape = EASMonsterHazardShape::Circle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard")
	EASMonsterHazardPlacement Placement = EASMonsterHazardPlacement::RandomAnnulus;

	/** PerPlayer 배치에서는 플레이어 한 명당 생성할 장판 수다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "1"))
	int32 Count = 6;

	/** 0이면 제한 없음. 멀티플레이에서 과도한 장판·Actor 생성을 막는 전체 상한이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0"))
	int32 MaxTotalHazards = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.05", Units = "s"))
	float TelegraphDuration = 1.0f;

	/** 이번 묶음이 폭발한 뒤 다음 묶음의 예고를 시작하기 전 간격. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", Units = "s"))
	float NextWaveDelay = 0.2f;

	/** Circle 반경. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "1.0", Units = "cm", EditCondition = "Shape == EASMonsterHazardShape::Circle", EditConditionHides))
	float CircleRadius = 180.0f;

	/** Cross의 각 막대 전체 길이. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "1.0", Units = "cm", EditCondition = "Shape == EASMonsterHazardShape::Cross", EditConditionHides))
	float CrossLength = 850.0f;

	/** Cross의 각 막대 전체 폭. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "1.0", Units = "cm", EditCondition = "Shape == EASMonsterHazardShape::Cross", EditConditionHides))
	float CrossWidth = 170.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", Units = "cm"))
	float MinPlacementRadius = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxPlacementRadius = 900.0f;

	/** PerPlayerPredictedPath에서 플레이어 속도를 앞당겨 읽을 시간. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", Units = "s", EditCondition = "Placement == EASMonsterHazardPlacement::PerPlayerPredictedPath", EditConditionHides))
	float PredictionLeadTime = 0.5f;

	/** 이동 예측 경로 양옆의 장판 간격. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "Placement == EASMonsterHazardPlacement::PerPlayerPredictedPath", EditConditionHides))
	float PathSpacing = 300.0f;

	/** 완전한 암기가 아닌 회피 판단을 요구하도록 더할 무작위 2D 오차. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "Placement == EASMonsterHazardPlacement::PerPlayerPredictedPath", EditConditionHides))
	float RandomJitter = 90.0f;

	/** 이동 예측 방향을 따라 각 십자 장판에 독립적으로 더할 앞뒤 위치 오차. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "Placement == EASMonsterHazardPlacement::PerPlayerPredictedPath", EditConditionHides))
	float ForwardJitter = 220.0f;

	/** 이동 방향을 기준으로 각 십자 장판에 독립적으로 더할 회전 오차의 최대 절대값. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg", EditCondition = "Placement == EASMonsterHazardPlacement::PerPlayerPredictedPath || Placement == EASMonsterHazardPlacement::PerPlayerSurround", EditConditionHides))
	float RotationJitterDegrees = 30.0f;

	/** PerPlayerSurround에서 플레이어 중심으로부터 각 장판 중심까지의 기본 거리. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "Placement == EASMonsterHazardPlacement::PerPlayerSurround", EditConditionHides))
	float SurroundRadius = 950.0f;

	/** PerPlayerSurround에서 장판별로 더할 반경 오차의 최대 절대값. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "Placement == EASMonsterHazardPlacement::PerPlayerSurround", EditConditionHides))
	float SurroundRadiusJitter = 100.0f;

	/** PerPlayerSurround의 균등 간격 위치에 각 장판별로 더할 각도 오차의 최대 절대값. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg", EditCondition = "Placement == EASMonsterHazardPlacement::PerPlayerSurround", EditConditionHides))
	float SurroundAngleJitterDegrees = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0"))
	float DamageScale = 1.0f;
};

/** 여러 원형/십자 장판을 웨이브 단위로 예고 후 폭발시키는 보스 기믹 Ability. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterHazardSequenceAbility : public UASMonsterComboStepAbility
{
	GENERATED_BODY()

public:
	UASMonsterHazardSequenceAbility();

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Hazard", meta = (TitleProperty = "Shape"))
	TArray<FASMonsterHazardWave> Waves;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Hazard")
	TSubclassOf<AASMonsterAttackIndicatorActor> IndicatorClass;

	/** PerPlayer 배치에서 공격 대상으로 고려할 최대 거리. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Hazard", meta = (ClampMin = "1.0", Units = "cm"))
	float PlayerSearchRadius = 3500.0f;

private:
	struct FHazardInstance
	{
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
	};

	void BeginNextWave();
	void ExplodeCurrentWave();
	void BuildCurrentWaveInstances(const FASMonsterHazardWave& Wave);
	TArray<AActor*> FindRelevantPlayers() const;
	void SpawnCurrentWaveIndicators(const FASMonsterHazardWave& Wave);
	void DestroyCurrentWaveIndicators();
	void ApplyCurrentWaveDamage(const FASMonsterHazardWave& Wave);
	void ApplyCircleDamage(const FHazardInstance& Instance, const FASMonsterHazardWave& Wave,
		FASCombatTargetHitSet& InOutHitTargets);
	void ApplyCrossDamage(const FHazardInstance& Instance, const FASMonsterHazardWave& Wave,
		FASCombatTargetHitSet& InOutHitTargets);
	FGameplayEffectSpecHandle MakeScaledDamageSpec(float DamageScale) const;

	int32 CurrentWaveIndex = INDEX_NONE;
	FVector SequenceOrigin = FVector::ZeroVector;
	FRandomStream RandomStream;
	TArray<FHazardInstance> CurrentWaveInstances;
	TArray<TWeakObjectPtr<AASMonsterAttackIndicatorActor>> ActiveIndicators;
	FTimerHandle WaveTelegraphTimerHandle;
	FTimerHandle NextWaveTimerHandle;
};
