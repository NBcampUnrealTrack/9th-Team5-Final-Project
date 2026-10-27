#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ASZoneDangerSchedule.generated.h"

/** 날짜별 낮/밤 위험도 증가 한도와 시작 시점 구역별 개체 수에 적용할 규칙. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASZoneDangerScheduleRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 날짜부터 다음 행의 날짜 전까지 적용한다. 마지막 행은 이후에도 계속 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger Schedule", meta = (ClampMin = "1"))
	int32 Day = 1;

	/** 낮 시작에 위험도를 올릴 수 있는 최대 구역 수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger Schedule", meta = (ClampMin = "0"))
	int32 MaxRaisedZonesDay = 1;

	/** 밤 시작에 위험도를 올릴 수 있는 최대 구역 수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger Schedule", meta = (ClampMin = "0"))
	int32 MaxRaisedZonesNight = 2;

	/**
	 * 시작 시점의 생존 몬스터 수가 이 값 이상이면 우선 증가 후보가 된다.
	 * 0이면 모든 증가 가능 구역을 몬스터 구역으로 보고, 추첨 확률 없이 몬스터 수 순으로 증가시킨다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger Schedule", meta = (ClampMin = "0"))
	int32 MonsterCountThreshold = 3;

	/** 시작 시점에 생존 플레이어가 없는 구역의 추가 추첨 확률. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger Schedule", meta = (ClampMin = "0", ClampMax = "1"))
	float BaseChance = 0.6f;

	/** 시작 시점에 생존 플레이어가 있는 구역의 추가 추첨 확률. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Danger Schedule", meta = (ClampMin = "0", ClampMax = "1"))
	float PlayerZoneChance = 0.8f;
};
