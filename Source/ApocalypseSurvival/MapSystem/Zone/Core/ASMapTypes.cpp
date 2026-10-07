#include "MapSystem/Zone/Core/ASMapTypes.h"

FName FASMapPhaseContext::GetPhaseKey() const
{
	return FName(*FString::Printf(TEXT("D%d_%s"), Day,
		DayPhase == EASDayPhase::Day ? TEXT("Day") : TEXT("Night")));
}

const FASZoneRuntimeState* FASMapStateSnapshot::FindZone(FName ZoneId) const
{
	return Zones.FindByPredicate([ZoneId](const FASZoneRuntimeState& State)
	{
		return State.ZoneId == ZoneId;
	});
}
