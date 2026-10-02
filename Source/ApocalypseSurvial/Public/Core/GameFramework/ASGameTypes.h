#pragma once

#include "CoreMinimal.h"
#include "ASGameTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePhase : uint8 { Ready, Day, Night, Victory, Defeat };

USTRUCT(BlueprintType)
struct FWaveResult
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly) int32 Day = 0;
	UPROPERTY(BlueprintReadOnly) int32 SpawnedCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 KilledCount = 0;
	UPROPERTY(BlueprintReadOnly) bool bAllCleared = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged, EGamePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveEnded, const FWaveResult&, Result);

// 밤 시작: AI 쪽이 구독해서 스폰 수를 결정 (bBossNight면 보스도 스폰)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNightStarted, int32 /*Day*/, bool /*bBossNight*/);

// 밤 종료
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNightEnded, int32 /*Day*/);