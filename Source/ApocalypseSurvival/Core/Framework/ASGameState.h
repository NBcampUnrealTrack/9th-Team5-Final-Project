#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/Framework/ASGameTypes.h"
#include "ASGameState.generated.h"

class UASMapStateComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamRevivesChanged, int32, RemainingRevives);

UCLASS()
class APOCALYPSESURVIVAL_API AASGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	AASGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 서버 전용 (GameMode가 호출)
	void SetPhase(EGamePhase NewPhase, float Duration);
	void SetCurrentDay(int32 NewDay);
	/** 서버가 확정한 스테이지 난이도 배율 입력값. 몬스터 스탯 Resolver가 모든 스폰에 공통 사용한다. */
	void SetMonsterStatDifficulty(float NewDifficulty);
	void ResetWaveCounts();
	void AddWaveSpawned(int32 Count);
	void AddWaveKilled();
	
	// 멀티 팀 공용 부활 횟수 (서버 전용)
	void SetTeamRevivesRemaining(int32 NewCount);
	
	// 1회 사용한다. 남은 횟수가 없으면 false (서버 전용)
	bool ConsumeTeamRevive();
	
	// 조회 (UI 등에서 사용)
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	EGamePhase GetCurrentPhase() const {return CurrentPhase;}
	
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	int32 GetCurrentDay() const {return CurrentDay;}

	UFUNCTION(BlueprintPure, Category = "GameFlow|Monster Stats")
	float GetMonsterStatDifficulty() const { return MonsterStatDifficulty; }
	
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	float GetPhaseRemainingTime() const;
	
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	int32 GetWaveSpawnedCount() const {return WaveSpawnedCount;}
	
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	int32 GetWaveKilledCount() const {return WaveKilledCount;}
	
	UFUNCTION(BlueprintPure, Category = "GameFlow|Revive")
	int32 GetTeamRevivesRemaining() const { return TeamRevivesRemaining; }
	
	// 남은 부활 횟수가 바뀔 때 (HUD 표시용, 서버·클라이언트 모두)
	UPROPERTY(BlueprintAssignable, Category = "GameFlow|Revive")
	FOnTeamRevivesChanged OnTeamRevivesChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "GameFlow")
	FOnPhaseChanged OnPhaseChanged;
	
protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase)
	EGamePhase CurrentPhase = EGamePhase::Ready;
	
	UPROPERTY(Replicated)
	int32 CurrentDay = 0;

	/** 1.0이 기본 난이도다. Curve 기반 성장에서 X 값으로 사용한다. */
	UPROPERTY(Replicated)
	float MonsterStatDifficulty = 1.0f;
	
	// 현재 단계가 끝나는 서버 시간 (0이면 시간 제한 없음)
	UPROPERTY(Replicated)
	float PhaseEndServerTime = 0.f;
	
	UPROPERTY(Replicated)
	int32 WaveSpawnedCount = 0;
	
	UPROPERTY(Replicated)
	int32 WaveKilledCount = 0;
	
	UPROPERTY(ReplicatedUsing = OnRep_TeamRevivesRemaining)
	int32 TeamRevivesRemaining = 0;
	
	UFUNCTION()
	void OnRep_TeamRevivesRemaining();
	
	UFUNCTION()
	void OnRep_CurrentPhase();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASMapStateComponent> MapState;
};
