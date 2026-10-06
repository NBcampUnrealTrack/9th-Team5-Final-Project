#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/Framework/ASGameTypes.h"
#include "ASGameState.generated.h"

UCLASS()
class APOCALYPSESURVIVAL_API AASGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 서버 전용 (GameMode가 호출)
	void SetPhase(EGamePhase NewPhase, float Duration);
	void SetCurrentDay(int32 NewDay);
	void ResetWaveCounts();
	void AddWaveSpawned(int32 Count);
	void AddWaveKilled();
	
	// 조회 (UI 등에서 사용)
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	EGamePhase GetCurrentPhase() const {return CurrentPhase;}
	
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	int32 GetCurrentDay() const {return CurrentDay;}
	
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	float GetPhaseRemainingTime() const;
	
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	int32 GetWaveSpawnedCount() const {return WaveSpawnedCount;}
	
	UFUNCTION(BlueprintPure, Category = "GameFlow")
	int32 GetWaveKilledCount() const {return WaveKilledCount;}
	
	UPROPERTY(BlueprintAssignable, Category = "GameFlow")
	FOnPhaseChanged OnPhaseChanged;
	
protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase)
	EGamePhase CurrentPhase = EGamePhase::Ready;
	
	UPROPERTY(Replicated)
	int32 CurrentDay = 0;
	
	// 현재 단계가 끝나는 서버 시간 (0이면 시간 제한 없음)
	UPROPERTY(Replicated)
	float PhaseEndServerTime = 0.f;
	
	UPROPERTY(Replicated)
	int32 WaveSpawnedCount = 0;
	
	UPROPERTY(Replicated)
	int32 WaveKilledCount = 0;
	
	UFUNCTION()
	void OnRep_CurrentPhase();
};
