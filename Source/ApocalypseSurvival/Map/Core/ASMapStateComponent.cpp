#include "Map/Core/ASMapStateComponent.h"

#include "GameFramework/Actor.h"
#include "Map/Core/ASMapDefinition.h"
#include "Net/UnrealNetwork.h"

UASMapStateComponent::UASMapStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UASMapStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UASMapStateComponent, Snapshot);
	DOREPLIFETIME(UASMapStateComponent, MapDefinition);
}

bool UASMapStateComponent::GetZoneState(FName ZoneId, FASZoneRuntimeState& OutState) const
{
	if (const FASZoneRuntimeState* State = Snapshot.FindZone(ZoneId))
	{
		OutState = *State;
		return true;
	}
	return false;
}

void UASMapStateComponent::ServerSetMapDefinition(UASMapDefinition* InMapDefinition)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		MapDefinition = InMapDefinition;
	}
}

void UASMapStateComponent::ServerWriteSnapshot(const FASMapStateSnapshot& NewSnapshot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const FASMapStateSnapshot OldSnapshot = Snapshot;
	Snapshot = NewSnapshot;

	// 리플리케이션은 클라이언트에서만 OnRep를 호출하므로 서버는 직접 브로드캐스트한다.
	BroadcastDifference(OldSnapshot, Snapshot);
}

void UASMapStateComponent::OnRep_Snapshot(const FASMapStateSnapshot& OldSnapshot)
{
	BroadcastDifference(OldSnapshot, Snapshot);
}

void UASMapStateComponent::BroadcastDifference(const FASMapStateSnapshot& OldSnapshot, const FASMapStateSnapshot& NewSnapshot)
{
	if (OldSnapshot.MatchId != NewSnapshot.MatchId)
	{
		OnMatchReset.Broadcast();
	}

	for (const FASZoneRuntimeState& NewState : NewSnapshot.Zones)
	{
		const FASZoneRuntimeState* OldState = OldSnapshot.FindZone(NewState.ZoneId);
		if (!OldState || *OldState != NewState)
		{
			OnZoneStateChanged.Broadcast(NewState.ZoneId, NewState);
		}
	}

	if (!OldSnapshot.bMapReady && NewSnapshot.bMapReady)
	{
		OnMapReady.Broadcast();
	}

	OnMapStateUpdated.Broadcast();
}
