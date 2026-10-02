#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/GameFramework/ASGameTypes.h"
#include "ASGameMode.generated.h"

class AASGameState;

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
	
	// 디버그 (콘솔에서 입력)
	UFUNCTION(Exec)
	void DebugSkipPhase();
	
protected:
	virtual void HandleMatchHasStarted() override;
	
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
	
	// 단계 진행
	void StartDay(int32 Day);
	void StartNight();
	void OnDayTimeUp();
	void OnNightTimeUp();
	void EndGame(bool bVictory);
	
	AASGameState* GetASGameState() const;
	
	FTimerHandle PhaseTimerHandle;
};
