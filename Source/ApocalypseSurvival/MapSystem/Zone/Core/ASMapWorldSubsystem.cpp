#include "MapSystem/Zone/Core/ASMapWorldSubsystem.h"

#include "Engine/World.h"
#include "MapSystem/Zone/ASMapZone.h"

UASMapWorldSubsystem* UASMapWorldSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UASMapWorldSubsystem>() : nullptr;
}

bool UASMapWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UASMapWorldSubsystem::RegisterZone(AASMapZone* Zone)
{
	if (!Zone)
	{
		return;
	}

	bSortedZonesDirty = true;
	Zones.RemoveAll([](const TWeakObjectPtr<AASMapZone>& Item) { return !Item.IsValid(); });
	if (!Zones.Contains(Zone))
	{
		Zones.Add(Zone);
		OnZoneRegistered.Broadcast(Zone);
	}
}

void UASMapWorldSubsystem::UnregisterZone(AASMapZone* Zone)
{
	bSortedZonesDirty = true;
	if (Zones.Remove(Zone) > 0)
	{
		OnZoneUnregistered.Broadcast(Zone);
	}
}

const TArray<AASMapZone*>& UASMapWorldSubsystem::GetZones() const
{
	if (bSortedZonesDirty)
	{
		SortedZones = ResolveWeak(Zones);
		SortedZones.Sort([](const AASMapZone& A, const AASMapZone& B)
		{
			return A.GetZoneId().LexicalLess(B.GetZoneId());
		});
		bSortedZonesDirty = false;
	}
	return SortedZones;
}

AASMapZone* UASMapWorldSubsystem::FindZone(FName ZoneId) const
{
	for (const TWeakObjectPtr<AASMapZone>& Zone : Zones)
	{
		if (Zone.IsValid() && Zone->GetZoneId() == ZoneId)
		{
			return Zone.Get();
		}
	}
	return nullptr;
}

AASMapZone* UASMapWorldSubsystem::FindZoneAtLocation(const FVector& Location) const
{
	for (AASMapZone* Zone : GetZones())
	{
		if (Zone->ContainsLocation(Location))
		{
			return Zone;
		}
	}
	return nullptr;
}

FName UASMapWorldSubsystem::FindZoneIdAtLocation(const FVector& Location) const
{
	const AASMapZone* Zone = FindZoneAtLocation(Location);
	return Zone ? Zone->GetZoneId() : NAME_None;
}
