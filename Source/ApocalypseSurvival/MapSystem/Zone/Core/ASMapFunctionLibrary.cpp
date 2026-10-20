#include "MapSystem/Zone/Core/ASMapFunctionLibrary.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MapSystem/Zone/Core/ASMapDefinition.h"
#include "MapSystem/Zone/Core/ASMapStateComponent.h"
#include "MapSystem/Zone/Core/ASMapWorldSubsystem.h"
#include "MapSystem/Zone/ASMapZone.h"

bool UASMapFunctionLibrary::LevelHasMapZones(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World && TActorIterator<AASMapZone>(World);
}

UASMapStateComponent* UASMapFunctionLibrary::GetMapStateComponent(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UASMapStateComponent>() : nullptr;
}

UASMapDefinition* UASMapFunctionLibrary::GetMapDefinition(const UObject* WorldContextObject)
{
	const UASMapStateComponent* MapState = GetMapStateComponent(WorldContextObject);
	return MapState ? MapState->GetMapDefinition() : nullptr;
}

FName UASMapFunctionLibrary::FindZoneIdAtLocation(const UObject* WorldContextObject, const FVector& Location)
{
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->FindZoneIdAtLocation(Location) : NAME_None;
}

TArray<FTransform> UASMapFunctionLibrary::GetZoneSpawnTransforms(const UObject* WorldContextObject, FName ZoneId)
{
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(WorldContextObject);
	const AASMapZone* Zone = Subsystem ? Subsystem->FindZone(ZoneId) : nullptr;
	return Zone ? Zone->GetSpawnTransforms() : TArray<FTransform>();
}

TArray<FTransform> UASMapFunctionLibrary::GetZoneRespawnTransforms(const UObject* WorldContextObject, FName ZoneId)
{
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(WorldContextObject);
	const AASMapZone* Zone = Subsystem ? Subsystem->FindZone(ZoneId) : nullptr;
	return Zone ? Zone->GetRespawnTransforms() : TArray<FTransform>();
}

FText UASMapFunctionLibrary::GetZoneDisplayName(const UObject* WorldContextObject, FName ZoneId)
{
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(WorldContextObject);
	const AASMapZone* Zone = Subsystem ? Subsystem->FindZone(ZoneId) : nullptr;
	return Zone ? Zone->GetDisplayName() : FText::FromName(ZoneId);
}

FLinearColor UASMapFunctionLibrary::GetDangerColor(const UObject* WorldContextObject, int32 DangerLevel)
{
	const UASMapDefinition* Definition = GetMapDefinition(WorldContextObject);
	return Definition ? Definition->GetDangerColor(DangerLevel) : FLinearColor::White;
}

FASZoneHudInfo UASMapFunctionLibrary::GetZoneHudInfoAtLocation(const UObject* WorldContextObject, const FVector& Location)
{
	FASZoneHudInfo Info;

	const FName ZoneId = FindZoneIdAtLocation(WorldContextObject, Location);
	if (ZoneId.IsNone())
	{
		return Info;
	}

	Info.bInZone = true;
	Info.ZoneId = ZoneId;
	Info.DisplayName = GetZoneDisplayName(WorldContextObject, ZoneId);

	const UASMapStateComponent* MapState = GetMapStateComponent(WorldContextObject);
	if (!MapState)
	{
		return Info;
	}

	const FASMapStateSnapshot& Snapshot = MapState->GetSnapshot();
	if (const FASZoneRuntimeState* State = Snapshot.FindZone(ZoneId))
	{
		Info.DangerLevel = State->DangerLevel;
	}
	Info.DangerColor = GetDangerColor(WorldContextObject, Info.DangerLevel);

	return Info;
}

APawn* UASMapFunctionLibrary::GetLocalPlayerPawn(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	return PlayerController && PlayerController->IsLocalController() ? PlayerController->GetPawn() : nullptr;
}
