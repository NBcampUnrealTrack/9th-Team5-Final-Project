#include "MapSystem/Zone/Core/ASMapRuleEngine.h"

#include "Math/RandomStream.h"
#include "Misc/Crc.h"

void FASMapRuleEngine::Configure(const TArray<FASZoneConfig>& InZones, float InDangerIncreaseChance)
{
	Zones = InZones;
	Zones.Sort([](const FASZoneConfig& A, const FASZoneConfig& B)
	{
		return A.ZoneId.LexicalLess(B.ZoneId);
	});
	DangerIncreaseChance = FMath::IsFinite(InDangerIncreaseChance)
		? FMath::Clamp(InDangerIncreaseChance, 0.0f, 1.0f) : 0.0f;
	ZoneIndex.Reset();
	for (int32 Index = 0; Index < Zones.Num(); ++Index)
	{
		ZoneIndex.Add(Zones[Index].ZoneId, Index);
	}
}

void FASMapRuleEngine::ResetMatch(int32 InMatchSeed, FASMapStateSnapshot& State)
{
	MatchSeed = InMatchSeed;
	ProcessedPhaseKeys.Reset();
	State.CurrentPhase = FASMapPhaseContext();
	State.Zones.Reset();
	for (const FASZoneConfig& Zone : Zones)
	{
		FASZoneRuntimeState& ZoneState = State.Zones.AddDefaulted_GetRef();
		ZoneState.ZoneId = Zone.ZoneId;
		ZoneState.DangerLevel = FMath::Clamp(Zone.InitialDangerLevel, ASMap::MinDangerLevel, ASMap::MaxDangerLevel);
	}
}

bool FASMapRuleEngine::BeginStep(FASMapStateSnapshot& State, const FASMapPhaseContext& Context, FASMapEngineEvents& OutEvents)
{
	const FName PhaseKey = Context.GetPhaseKey();
	if (ProcessedPhaseKeys.Contains(PhaseKey))
	{
		return false;
	}
	ProcessedPhaseKeys.Add(PhaseKey);
	State.CurrentPhase = Context;

	// FName 내부 인덱스 대신 문자열 CRC를 사용해 같은 매치와 스텝을 재현한다.
	const FString StepString = PhaseKey.ToString().ToLower();
	const uint32 StepHash = FCrc::StrCrc32(*StepString);
	const uint32 Seed = FCrc::MemCrc32(&MatchSeed, sizeof(MatchSeed), StepHash);
	FRandomStream Stream(static_cast<int32>(Seed));

	// 설정은 ZoneId 순이다. 최대 위험도 구역도 추첨해 뒤 구역의 난수 순서를 유지한다.
	for (const FASZoneConfig& Config : Zones)
	{
		const float Roll = Stream.FRand();
		FASZoneRuntimeState* ZoneState = State.Zones.FindByPredicate([&Config](const FASZoneRuntimeState& Zone)
		{
			return Zone.ZoneId == Config.ZoneId;
		});
		if (ZoneState && Roll < DangerIncreaseChance && ZoneState->DangerLevel < ASMap::MaxDangerLevel)
		{
			++ZoneState->DangerLevel;
			OutEvents.ChangedZones.Add(Config.ZoneId);
		}
	}
	return true;
}

const FASZoneConfig* FASMapRuleEngine::FindZoneConfig(FName ZoneId) const
{
	const int32* Index = ZoneIndex.Find(ZoneId);
	return Index ? &Zones[*Index] : nullptr;
}
