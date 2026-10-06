#pragma once

#include "CoreMinimal.h"
#include "ASMapTypes.generated.h"

namespace ASMap
{
	constexpr int32 MinDangerLevel = 1;
	constexpr int32 MaxDangerLevel = 5;
}

/** 게임 하루의 낮/밤 구분. GameMode가 시계를 소유하고 맵은 반응만 한다. */
UENUM(BlueprintType)
enum class EASDayPhase : uint8
{
	Day,
	Night
};

/** GameMode가 결정해 맵 매니저에 전달하는 단계 정보. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASMapPhaseContext
{
	GENERATED_BODY()

	/** 1부터 시작하는 게임 날짜. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	int32 Day = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	EASDayPhase DayPhase = EASDayPhase::Day;

	/** 이번 단계의 고유 키. 중복 제거와 시드 생성에 사용한다. */
	FName GetPhaseKey() const;

	bool operator==(const FASMapPhaseContext& Other) const
	{
		return Day == Other.Day && DayPhase == Other.DayPhase;
	}

	bool operator!=(const FASMapPhaseContext& Other) const { return !(*this == Other); }
};

/** 단일 구역의 현재 공유 상태. UASMapStateComponent를 통해 복제된다. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASZoneRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FName ZoneId;

	/** 구역 위험도 (1..5). */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 DangerLevel = ASMap::MinDangerLevel;

	bool operator==(const FASZoneRuntimeState& Other) const
	{
		return ZoneId == Other.ZoneId
			&& DangerLevel == Other.DangerLevel;
	}

	bool operator!=(const FASZoneRuntimeState& Other) const { return !(*this == Other); }
};

/** 클라이언트가 맵 UI를 재구성하는 데 필요한 모든 정보. 단일 프로퍼티로 복제된다. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASMapStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	TArray<FASZoneRuntimeState> Zones;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FASMapPhaseContext CurrentPhase;

	/** StartMap마다 증가하여 클라이언트가 새 매치를 식별할 수 있다. */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 MatchId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bMapReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bMapRunning = false;

	const FASZoneRuntimeState* FindZone(FName ZoneId) const;
};

/** HUD 위젯을 위한 특정 위치의 간편 정보. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASZoneHudInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bInZone = false;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FName ZoneId;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 DangerLevel = ASMap::MinDangerLevel;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FLinearColor DangerColor = FLinearColor::White;

	bool operator==(const FASZoneHudInfo& Other) const
	{
		return bInZone == Other.bInZone
			&& ZoneId == Other.ZoneId
			&& DangerLevel == Other.DangerLevel
			&& DisplayName.EqualTo(Other.DisplayName)
			&& DangerColor == Other.DangerColor;
	}

	bool operator!=(const FASZoneHudInfo& Other) const { return !(*this == Other); }
};
