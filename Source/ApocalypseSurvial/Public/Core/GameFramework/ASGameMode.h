#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/GameFramework/ASGameTypes.h"
#include "ASGameMode.generated.h"

class AASGameState;
class AASRespawnPoint;
class UASPlayerRecordComponent;

UCLASS()
class APOCALYPSESURVIAL_API AASGameMode : public AGameMode
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
	
	// 플레이어가 죽었을 때 호출 (캐릭터 쪽에서 서버에서 호출)
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void OnPlayerDied(AController* DeadController);
	
	// 맵에 배치된 리스폰 포인트 목록
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	TArray<AASRespawnPoint*> GetAvailableRespawnPoints() const;
	
	// 밤이 끝날 때 웨이브 결과 (보상 시스템 등이 참조)
	UPROPERTY(BlueprintAssignable, Category = "GameFlow")
	FOnWaveEnded OnWaveEnded;
	
	// 스폰 위치 (AI쪽에서 호출)
	// 살아 있는 플레이어 중 한 명 주변에서 스폰 위치를 찾음 (설정값의 최소·최대 거리 사용)
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	bool FindSpawnLocation(FVector& OutLocation) const;
    
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
	
	// 사망 후 리스폰까지 걸리는 시간 (초)
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
	
	// 단계 진행
	void StartDay(int32 Day);
	void StartNight();
	void OnDayTimeUp();
	void OnNightTimeUp();
	void EndGame(bool bVictory);
	
	bool IsGameEnded() const;
	FWaveResult BuildWaveResult() const;
	
	// PlayerState의 기록 컴포넌트를 찾고, 없으면 붙여서 반환
	UASPlayerRecordComponent* GetOrCreateRecord(AController* Controller) const;
	
	// 리스폰 실행
	void RespawnPlayer(AController* Controller);
	
	AASGameState* GetASGameState() const;
	
	FTimerHandle PhaseTimerHandle;
};
