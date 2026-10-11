#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASSpawnSubsystem.generated.h"

class AASMonsterCharacter;
class AASMonsterSpawnPoint;
class UDataTable;
struct FASDayMonsterGroupSelectionEntry;

/** 맵 배치 몬스터와 밤 웨이브의 위치 선정·생성·추적을 전담하는 서버 월드 Subsystem. */
UCLASS(Config = Game, DefaultConfig)
class APOCALYPSESURVIVAL_API UASSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	void HandleDayStarted(int32 Day, UDataTable* DaySpawnDataTable, UDataTable* MonsterSpawnGroupDataTable);
	void HandleNightStarted(int32 Day, UDataTable* WaveDataTable);
	void HandleNightEnded();
	void StopAllSpawning();

private:
	void SpawnDayUnits(int32 Day, UDataTable* DaySpawnDataTable, UDataTable* MonsterSpawnGroupDataTable);
	int32 SpawnDayGroup(const FASDayMonsterGroupSelectionEntry& GroupSelection, UDataTable* MonsterSpawnGroupDataTable,
		const AASMonsterSpawnPoint& SpawnPoint);
	void StartWave(int32 Day, UDataTable* WaveDataTable);
	void StopWave();
	void SpawnWaveTick();

	void CachePlacedSpawnPoints();
	TArray<AASMonsterSpawnPoint*> SelectPlacedSpawnPoints(int32 Count) const;
	TArray<FVector> FindDistributedSpawnLocations(const AASMonsterSpawnPoint& SpawnPoint, int32 Count) const;
	const struct FASMonsterSpawnGroupDataRow* FindMonsterGroupData(UDataTable* MonsterSpawnGroupDataTable,
		int32 GroupIndex) const;
	bool FindWaveSpawnLocations(int32 EnemyCount, TArray<FVector>& OutLocations);
	void GenerateWaveSpawnPoints();
	bool IsNearAnyLivingPlayer(const FVector& Location) const;
	AASMonsterCharacter* SpawnMonster(TSubclassOf<AASMonsterCharacter> MonsterClass, const FTransform& SpawnTransform,
		bool bCanAggroDuringDay, const AASMonsterSpawnPoint* HomeSpawnPoint = nullptr);
	int32 CountAliveManagedMonsters();

	/** 플레이어와 충분히 떨어진 곳에 만들 밤 웨이브 후보 위치 설정. */
	UPROPERTY(Config, EditAnywhere, Category = "Spawn|Wave", meta = (ClampMin = "0.0", Units = "cm"))
	float WaveSpawnMinDistance = 2500.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Spawn|Wave", meta = (ClampMin = "1"))
	int32 WaveSpawnPointCount = 30;

	UPROPERTY(Config, EditAnywhere, Category = "Spawn|Wave", meta = (ClampMin = "0.0", Units = "cm"))
	float WaveSpawnPointMinSpacing = 1500.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Spawn|Wave", meta = (ClampMin = "1"))
	int32 WaveEnemiesPerSpawnPoint = 5;

	UPROPERTY(Config, EditAnywhere, Category = "Spawn|Wave", meta = (ClampMin = "0.0", Units = "cm"))
	float WaveSpawnScatterRadius = 300.0f;

	TArray<TWeakObjectPtr<AASMonsterSpawnPoint>> PlacedSpawnPoints;
	TArray<FVector> GeneratedWaveSpawnPoints;
	TSet<TWeakObjectPtr<AASMonsterCharacter>> ManagedMonsters;

	FTimerHandle WaveSpawnTimerHandle;
	int32 TotalWaveMonsterCount = 0;
	int32 SpawnedWaveMonsterCount = 0;
	int32 ActiveWaveSpawnBatchSize = 1;
	float ActiveWaveSpawnInterval = 0.25f;
	TArray<TSubclassOf<AASMonsterCharacter>> PendingWaveMonsterClasses;
};
