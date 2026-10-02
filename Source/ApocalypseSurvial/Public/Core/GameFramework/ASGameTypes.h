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