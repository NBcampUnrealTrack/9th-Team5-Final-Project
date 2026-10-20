#pragma once

#include "CoreMinimal.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"
#include "MapSystem/Zone/Core/ASZoneDangerSchedule.h"

/** 등록된 구역 액터의 초기 위험도와 양방향으로 병합한 인접 관계. */
struct APOCALYPSESURVIVAL_API FASZoneConfig
{
	FName ZoneId;
	int32 InitialDangerLevel = ASMap::MinDangerLevel;
	TArray<FName> AdjacentZoneIds;
};

/** 낮/밤 시작 시점의 구역별 생존 몬스터 수와 플레이어 존재. 누락된 구역은 기본값으로 취급한다. */
struct APOCALYPSESURVIVAL_API FASZonePhaseCounts
{
	int32 MonsterCount = 0;
	bool bHasPlayers = false;
};

/** 호출 한 번에서 위험도가 변경된 구역. */
struct APOCALYPSESURVIVAL_API FASMapEngineEvents
{
	TSet<FName> ChangedZones;
	bool HasStateChange() const { return ChangedZones.Num() > 0; }
};

/**
 * 낮/밤 시작마다 날짜별 한도 안에서 위험도를 1씩 올리는 순수 규칙 엔진.
 * 현재 생존 몬스터 수가 많은 순으로 우선 배정하고 남은 한도는 시드 추첨으로 채운다.
 * 현재 상태는 호출자가 전달하는 공유 스냅샷에만 저장한다.
 * 엔진은 정적 설정, 매치 시드와 처리한 스텝 키만 유지한다.
 */
class APOCALYPSESURVIVAL_API FASMapRuleEngine
{
public:
	void Configure(const TArray<FASZoneConfig>& InZones);

	/** 구역을 초기 위험도로 되돌리고 스텝 중복 기록을 지운다. MatchId와 공유 옵션은 유지한다. */
	void ResetMatch(int32 InMatchSeed, FASMapStateSnapshot& State);

	/** 날짜와 낮/밤으로 식별하는 스텝. 이미 처리한 키이면 상태를 수정하지 않고 false를 반환한다. */
	bool BeginStep(FASMapStateSnapshot& State, const FASMapPhaseContext& Context,
		const FASZoneDangerScheduleRow& Rule, const TMap<FName, FASZonePhaseCounts>& PhaseCounts,
		FASMapEngineEvents& OutEvents);

	const FASZoneConfig* FindZoneConfig(FName ZoneId) const;
	bool IsZoneKnown(FName ZoneId) const { return ZoneIndex.Contains(ZoneId); }

private:
	TArray<FASZoneConfig> Zones;
	TMap<FName, int32> ZoneIndex;
	TSet<FName> ProcessedPhaseKeys;
	int32 MatchSeed = 0;
};
