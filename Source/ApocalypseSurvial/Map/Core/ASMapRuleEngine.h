#pragma once

#include "CoreMinimal.h"
#include "Map/Core/ASMapTypes.h"

/** 등록된 구역 액터의 초기 위험도와 양방향으로 병합한 인접 관계. */
struct APOCALYPSESURVIAL_API FASZoneConfig
{
	FName ZoneId;
	int32 InitialDangerLevel = ASMap::MinDangerLevel;
	TArray<FName> AdjacentZoneIds;
};

/** 호출 한 번에서 위험도가 변경된 구역. */
struct APOCALYPSESURVIAL_API FASMapEngineEvents
{
	TSet<FName> ChangedZones;
	bool HasStateChange() const { return ChangedZones.Num() > 0; }
};

/**
 * 낮/밤 시작마다 구역별 위험도를 즉시 증가시키는 순수 규칙 엔진.
 * 현재 상태는 호출자가 전달하는 공유 스냅샷에만 저장한다.
 * 엔진은 정적 설정, 매치 시드와 처리한 스텝 키만 유지한다.
 */
class APOCALYPSESURVIAL_API FASMapRuleEngine
{
public:
	void Configure(const TArray<FASZoneConfig>& InZones, float InDangerIncreaseChance);

	/** 구역을 초기 위험도로 되돌리고 스텝 중복 기록을 지운다. MatchId와 공유 옵션은 유지한다. */
	void ResetMatch(int32 InMatchSeed, FASMapStateSnapshot& State);

	/** 날짜와 낮/밤으로 식별하는 스텝. 이미 처리한 키이면 상태를 수정하지 않고 false를 반환한다. */
	bool BeginStep(FASMapStateSnapshot& State, const FASMapPhaseContext& Context, FASMapEngineEvents& OutEvents);

	const FASZoneConfig* FindZoneConfig(FName ZoneId) const;
	bool IsZoneKnown(FName ZoneId) const { return ZoneIndex.Contains(ZoneId); }

private:
	TArray<FASZoneConfig> Zones;
	TMap<FName, int32> ZoneIndex;
	TSet<FName> ProcessedPhaseKeys;
	int32 MatchSeed = 0;
	float DangerIncreaseChance = 0.7f;
};
