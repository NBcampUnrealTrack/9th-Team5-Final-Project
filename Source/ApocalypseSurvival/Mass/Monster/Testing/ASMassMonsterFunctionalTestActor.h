#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "Mass/Monster/Data/ASMassMonsterTypes.h"
#include "ASMassMonsterFunctionalTestActor.generated.h"

class UAbilitySystemComponent;
class UMassEntityConfigAsset;

/** Phase 8 기능 동등성 검증용 결정적 Mass 몬스터 Batch Spawn Driver. */
UCLASS()
class APOCALYPSESURVIVAL_API AASMassMonsterFunctionalTestActor : public AActor
{
	GENERATED_BODY()

public:
	AASMassMonsterFunctionalTestActor();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 시야 Stress 중 Entity별 로컬 안개 표시 상태 변화를 추적한다. */
	struct FVisibilityStressTrack
	{
		bool bHasState = false;
		bool bVisible = false;
		double LastToggleTime = -1.0;
		int32 ToggleCount = 0;
		uint8 VisitedStateMask = 0;
	};

	void StartVisibilityStress();
	void HoldMonstersInPlace();
	void SampleVisibilityStress();
	void ReportVisibilityStress();
	void ToggleStressDayNight();
	void FinishVisibilityStress();

	void TryStartTest();
	void SpawnNextBatch();
	void HandleSpawnComplete();
	void ReportStateCounts();
	void StartCsvCapture();
	void StopCsvCapture();
	void StartTraceCapture();
	void StopTraceCapture();
	void FinishBenchmark();
	void ExecuteBenchmarkCommand(const FString& Command);
	bool BuildSpawnTransforms();
	void GrantPlayerInvulnerability();
	void RemoveGrantedInvulnerability();

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test")
	TObjectPtr<UMassEntityConfigAsset> EntityConfig;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test", meta = (ClampMin = "0", ClampMax = "500"))
	int32 MonsterCount = 10;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test")
	EASMassMonsterWorldBehavior WorldBehavior = EASMassMonsterWorldBehavior::NightAssault;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Spawn", meta = (ClampMin = "0.0", Units = "s"))
	float InitialDelay = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Spawn", meta = (ClampMin = "1"))
	int32 SpawnBatchSize = 10;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Spawn", meta = (ClampMin = "0.01", Units = "s"))
	float SpawnInterval = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSpawnRadius = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Spawn", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxSpawnRadius = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test")
	bool bGrantInvulnerability = true;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test", meta = (ClampMin = "0.0", Units = "s"))
	float StateReportDelay = 5.0f;

	/** 스폰 안정화 이후 CSV와 Insights Trace를 서로 겹치지 않게 자동 수집한다. */
	UPROPERTY(EditAnywhere, Category = "Mass Benchmark")
	bool bEnableBenchmarkCapture = false;

	UPROPERTY(EditAnywhere, Category = "Mass Benchmark", meta = (ClampMin = "0.0", Units = "s"))
	float BenchmarkSettleDelay = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Mass Benchmark", meta = (ClampMin = "1.0", Units = "s"))
	float CsvCaptureDuration = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Mass Benchmark")
	bool bCaptureTrace = true;

	UPROPERTY(EditAnywhere, Category = "Mass Benchmark", meta = (ClampMin = "1.0", Units = "s"))
	float TraceCaptureDuration = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Mass Benchmark", meta = (ClampMin = "0.0", Units = "s"))
	float BetweenCaptureDelay = 1.0f;

	/**
	 * 낮/밤 시야 반경 변화로 고정된 몬스터 위를 시야 경계가 반복 통과하게 하고,
	 * 밝은 시야/숨긴 Buffer/컬링 상태 Counter와 깜빡임(짧은 시간 내 표시 상태 왕복)을 기록한다.
	 * 빠른 낮/밤 주기는 FunctionalTest 전용 GameMode에서만 설정한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Visibility Stress")
	bool bEnableVisibilityStress = false;

	/** 스폰된 몬스터의 탐지 반경을 0으로 바꿔 플레이어를 쫓지 않고 제자리에 머물게 한다. */
	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Visibility Stress")
	bool bHoldMonstersInPlace = true;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Visibility Stress", meta = (ClampMin = "1.0", Units = "s"))
	float VisibilityStressDuration = 32.0f;

	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Visibility Stress", meta = (ClampMin = "0.05", Units = "s"))
	float VisibilityReportInterval = 0.5f;

	/** 이 시간 안에 표시 상태가 되돌아가면 깜빡임으로 집계한다. */
	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Visibility Stress", meta = (ClampMin = "0.0", Units = "s"))
	float FlickerWindowSeconds = 0.5f;

	/**
	 * 테스트 맵 전용 낮/밤 주기. GameMode의 실제 낮/밤 시간은 바꾸지 않고, 안개 관리자의 밤 상태만
	 * 이 간격으로 번갈아 바꿔 시야 반경(낮/밤)과 경계를 빠르게 반복 이동시킨다. 0이면 사용하지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Mass Functional Test|Visibility Stress", meta = (ClampMin = "0.0", Units = "s"))
	float StressDayNightHalfCycleSeconds = 0.0f;

	TMap<FMassEntityHandle, FVisibilityStressTrack> VisibilityStressTracks;
	FTimerHandle VisibilityReportTimer;
	FTimerHandle VisibilityStressEndTimer;
	FTimerHandle StressDayNightTimer;
	bool bStressNight = false;
	int32 StressDayNightToggleCount = 0;
	double VisibilityStressStartTime = 0.0;
	int32 VisibilityFlickerEvents = 0;

	TArray<FTransform> PendingSpawnTransforms;
	TArray<TWeakObjectPtr<UAbilitySystemComponent>> ModifiedAbilitySystems;
	int32 NextSpawnIndex = 0;
	FTimerHandle StartTimer;
	FTimerHandle SpawnBatchTimer;
	FTimerHandle StateReportTimer;
	FTimerHandle BenchmarkSettleTimer;
	FTimerHandle CsvCaptureTimer;
	FTimerHandle BetweenCaptureTimer;
	FTimerHandle TraceCaptureTimer;
	bool bCsvCaptureActive = false;
	bool bTraceCaptureActive = false;
};
