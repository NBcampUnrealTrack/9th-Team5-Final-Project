#include "MapSystem/Zone/Core/ASMapRuleEngine.h"

#include "Math/RandomStream.h"
#include "Misc/Crc.h"

void FASMapRuleEngine::Configure(const TArray<FASZoneConfig>& InZones)
{
	Zones = InZones;
	Zones.Sort([](const FASZoneConfig& A, const FASZoneConfig& B)
	{
		return A.ZoneId.LexicalLess(B.ZoneId);
	});
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

bool FASMapRuleEngine::BeginStep(FASMapStateSnapshot& State, const FASMapPhaseContext& Context,
	const FASZoneDangerScheduleRow& Rule, const TMap<FName, FASZonePhaseCounts>& PhaseCounts,
	FASMapEngineEvents& OutEvents)
{
	const FName PhaseKey = Context.GetPhaseKey();
	if (ProcessedPhaseKeys.Contains(PhaseKey))
	{
		return false;
	}
	ProcessedPhaseKeys.Add(PhaseKey);
	State.CurrentPhase = Context;
	const int32 Quota = Context.DayPhase == EASDayPhase::Night ? Rule.MaxRaisedZonesNight : Rule.MaxRaisedZonesDay;
	if (Quota <= 0)
	{
		return true;
	}

	TArray<FASZoneRuntimeState*> EligibleZones;
	TArray<FASZoneRuntimeState*> MonsterZones;
	for (const FASZoneConfig& Config : Zones)
	{
		FASZoneRuntimeState* ZoneState = State.Zones.FindByPredicate([&Config](const FASZoneRuntimeState& Zone)
		{
			return Zone.ZoneId == Config.ZoneId;
		});
		if (!ZoneState || ZoneState->DangerLevel >= ASMap::MaxDangerLevel)
		{
			continue;
		}
		EligibleZones.Add(ZoneState);
		// 첫 스텝도 현재 생존 몬스터 수로 우선 후보를 정한다.
		if (PhaseCounts.FindRef(Config.ZoneId).MonsterCount >= Rule.MonsterCountThreshold)
		{
			MonsterZones.Add(ZoneState);
		}
	}
	MonsterZones.Sort([&PhaseCounts](const FASZoneRuntimeState& A, const FASZoneRuntimeState& B)
	{
		const int32 CountA = PhaseCounts.FindRef(A.ZoneId).MonsterCount;
		const int32 CountB = PhaseCounts.FindRef(B.ZoneId).MonsterCount;
		return CountA == CountB ? A.ZoneId.LexicalLess(B.ZoneId) : CountA > CountB;
	});

	TSet<FName> RaisedZones;
	for (FASZoneRuntimeState* Zone : MonsterZones)
	{
		++Zone->DangerLevel;
		RaisedZones.Add(Zone->ZoneId);
		OutEvents.ChangedZones.Add(Zone->ZoneId);
		if (RaisedZones.Num() == Quota)
		{
			return true;
		}
	}
	EligibleZones.RemoveAll([&RaisedZones](const FASZoneRuntimeState* Zone)
	{
		return RaisedZones.Contains(Zone->ZoneId);
	});

	// FName 내부 인덱스 대신 문자열 CRC를 사용해 같은 매치와 스텝을 재현한다.
	const FString StepString = PhaseKey.ToString().ToLower();
	const uint32 StepHash = FCrc::StrCrc32(*StepString);
	const uint32 Seed = FCrc::MemCrc32(&MatchSeed, sizeof(MatchSeed), StepHash);
	FRandomStream Stream(static_cast<int32>(Seed));

	// ZoneId 순에서 시작해 같은 스트림으로 Fisher-Yates 셔플 후 확률을 추첨한다.
	for (int32 Index = EligibleZones.Num() - 1; Index > 0; --Index)
	{
		EligibleZones.Swap(Index, Stream.RandRange(0, Index));
	}
	for (FASZoneRuntimeState* Zone : EligibleZones)
	{
		const float Chance = PhaseCounts.FindRef(Zone->ZoneId).bHasPlayers ? Rule.PlayerZoneChance : Rule.BaseChance;
		if (Stream.FRand() < Chance)
		{
			++Zone->DangerLevel;
			RaisedZones.Add(Zone->ZoneId);
			OutEvents.ChangedZones.Add(Zone->ZoneId);
			if (RaisedZones.Num() == Quota)
			{
				break;
			}
		}
	}
	return true;
}

const FASZoneConfig* FASMapRuleEngine::FindZoneConfig(FName ZoneId) const
{
	const int32* Index = ZoneIndex.Find(ZoneId);
	return Index ? &Zones[*Index] : nullptr;
}
