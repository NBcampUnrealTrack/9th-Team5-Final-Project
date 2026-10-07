#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/Framework/ASGameTypes.h"
#include "ASGameMode.generated.h"

class AASGameState;
class AASRespawnPoint;
class UASPlayerRecordComponent;
class UDataTable;

UCLASS()
class APOCALYPSESURVIVAL_API AASGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AASGameMode();
	
	// 규칙 조회 (일차별 데이터 교체 가능)
	virtual float GetDayDuration(int32 Day) const;
	virtual float GetNightDuration(int32 Day) const;
	virtual bool IsBossDay(int32 Day) const;
	virtual bool IsDeathCountEnabled() const;
	
	// 다른 시스템이 구독하는 이벤트 (서버 전용)
	FOnNightStarted OnNightStarted;
	FOnNightEnded OnNightEnded;
	
	// AI쪽에서 호출 (서버 전용)
	// 이번 밤에 스폰한 적 수를 알려줌 (웨이브 결과 계산용)
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void ReportWaveSpawned(int32 Count);
	
	// 적이 죽었을 때 호출. Killer는 처치한 플레이어의 컨트롤러 (없으면 nullptr)
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void OnEnemyKilled(AController* Killer);
	
	// 보스가 죽었을 때 호출
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void OnBossDefeated();
	
	// 밤이 끝날 때 웨이브 결과 (보상 시스템 등이 참조)
	UPROPERTY(BlueprintAssignable, Category = "GameFlow")
	FOnWaveEnded OnWaveEnded;
	
	// 사망과 리스폰 (플레이어 쪽에서 호출)
	// 플레이어가 죽었을 때 호출. 데스카운트 초과면 패배, 아니면 리스폰
	UFUNCTION(BlueprintCallable, Category = "Death")
	void OnPlayerDied(AController* DeadController);
		
	// 맵에 배치된 리스폰 포인트 목록
	TArray<AASRespawnPoint*> GetAvailableRespawnPoints() const;
		
	
	// 웨이브용: 적 수만큼의 스폰 위치를 한 번에 반환(동시 스폰)
	// 플레이어 근처 포인트는 제외하고, 남은 포인트 중 랜덤으로 적 수에 비례한 개수를 골라 나눠 배치
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	bool FindSpawnLocation(int32 EnemyCount, TArray<FVector>& OutLocations) const;
    
	// 지정한 위치 주변에서 스폰 위치를 찾음 (거리를 직접 지정)
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	bool FindSpawnLocationAround(const FVector& Center,float MinDistance, float MaxDistance, FVector& OutLocation) const;
    
	// 살아 있는 플레이어 Pawn 중 하나를 랜덤으로 반환 (없으면 nullptr)
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	APawn* GetRandomAlivePlayerPawn() const;
    
    
	// 디버그 (콘솔에서 입력)
	UFUNCTION(Exec)
	void DebugSkipPhase();
	
	UFUNCTION(Exec)
	void DebugSpawnWave(int32 Count);
	
	UFUNCTION(Exec)
	void DebugKillEnemy();
	
	UFUNCTION(Exec)
	void DebugKillBoss();
	
	UFUNCTION(Exec)
	void DebugShowSpawnPoints(int32 Count);
	
	UFUNCTION(Exec)
	void DebugKillPlayer();
	
	UFUNCTION(Exec)
	void DebugShowEnemySpawnPoints(int32 EnemyCount);
	
protected:
	virtual void HandleMatchHasStarted() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	// 게임 시작 전 대기 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Time")
	float ReadyDuration = 3.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Time")
	float DayDuration = 140.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Time")
	float NightDuration = 110.f;
	
	// 보스 등장일
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Boss")
	TArray<int32> BossDays;
	
	// 싱글 전용
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn")
	int32 MaxDeathCount = 1;
	
	// 0 = 무제한
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn")
	float RespawnSelectTimeLimit = 0.f;
	
	// 사망 후 리스폰까지 대기 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn")
	float RespawnDelay = 3.f;
	
	// 플레이어로부터 최소 스폰 거리 (cm, 화면 밖)
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Spawn")
	float SpawnMinDistance = 2500.f;
	
	// 플레이어로부터 최대 스폰 거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Spawn")
	float SpawnMaxDistance = 4000.f;
	
	// 위치를 찾을 때 시도 횟수
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Spawn")
	int32 SpawnFindAttempts = 10;
	
	// 게임 시작 시 자동 생성할 적 스폰 포인트 수
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Spawn")
	int32 SpawnPointCount = 30;
	
	// 스폰 포인트끼리 최소 간격 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Spawn")
	float SpawnPointMinSpacing = 1500.f;
	
	// 스폰 포인트 1개당 적 수 (적 수에 비례해서 사용할 포인트 수 결정)
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Spawn")
	int32 EnemiesPerSpawnPoint = 5;
	
	// 포인트 주변에 적을 흩뿌리는 반경 (cm, 겹침 방지)
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Spawn")
	float SpawnScatterRadius = 300.f;

	// 행 이름 Wave_01, Wave_02 ... 에 따라 날짜별 스웜을 정의한 DataTable.
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Wave")
	TObjectPtr<UDataTable> WaveDataTable;
	
	// 단계 진행
	void StartDay(int32 Day);
	void StartNight();
	void OnDayTimeUp();
	void OnNightTimeUp();
	void EndGame(bool bVictory);
	
	// 사망과 리스폰
	UASPlayerRecordComponent* GetOrCreateRecord(AController* Controller) const;
	void RespawnPlayer(AController* Controller);
	
	// 적 스폰 포인트
	void GenerateEnemySpawnPoints();
	bool IsNearAnyPlayer(const FVector& Location) const;
	TArray<FVector> GetAvailableEnemySpawnPoints() const;
	
	// 게임 시작 시 자동 생성된 적 스폰 포인트
	TArray<FVector> EnemySpawnPoints;
	
	// 리스폰 시 바닥에 박히지 않도록 높이 보간 (캡슐 절반 높이, 없으면 100cm)
	float GetSpawnHeightOffset(AController* Controller);
	
	bool IsGameEnded() const;
	FWaveResult BuildWaveResult() const;
	
	AASGameState* GetASGameState() const;
	
	FTimerHandle PhaseTimerHandle;
};
