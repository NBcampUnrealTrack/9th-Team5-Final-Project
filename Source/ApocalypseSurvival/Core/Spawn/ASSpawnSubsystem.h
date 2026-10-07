#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASSpawnSubsystem.generated.h"

class AASMonsterCharacter;
class UDataTable;

/** 밤 웨이브의 DataTable 해석과 스웜 생성을 전담하는 서버 월드 Subsystem. */
UCLASS()
class APOCALYPSESURVIVAL_API UASSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void StartWave(int32 Day, UDataTable* WaveDataTable);
	void StopWave();

private:
	void SpawnWaveTick();

	FTimerHandle WaveSpawnTimerHandle;
	int32 TotalWaveMonsterCount = 0;
	int32 SpawnedWaveMonsterCount = 0;
	int32 ActiveWaveSpawnBatchSize = 1;
	float ActiveWaveSpawnInterval = 0.25f;
	TArray<TSubclassOf<AASMonsterCharacter>> PendingWaveMonsterClasses;
};
