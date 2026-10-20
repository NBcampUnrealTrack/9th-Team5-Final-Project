#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
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

	UPROPERTY(EditAnywhere, Category = "Mass Benchmark")
	bool bTakeScreenshot = true;

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
