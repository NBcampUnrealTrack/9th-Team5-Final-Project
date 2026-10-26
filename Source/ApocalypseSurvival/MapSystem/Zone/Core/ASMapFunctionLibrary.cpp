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

namespace ASMapFunctionLibraryPrivate
{
	TArray<FASZoneSpawnPoint> GetZonePoints(const UObject* WorldContextObject, FName ZoneId, bool bRespawn)
	{
		TArray<FASZoneSpawnPoint> Result;
		const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(WorldContextObject);
		if (!Subsystem)
		{
			return Result;
		}

		const UASMapStateComponent* MapState = UASMapFunctionLibrary::GetMapStateComponent(WorldContextObject);
		const FASMapStateSnapshot* Snapshot = MapState ? &MapState->GetSnapshot() : nullptr;
		const auto AppendZonePoints = [&](const AASMapZone* Zone)
		{
			if (!IsValid(Zone))
			{
				return;
			}

			// 구역 정보는 한 번만 조회하고 각 월드 공간 지점에 복사한다.
			FASZoneSpawnPoint Info;
			Info.ZoneId = Zone->GetZoneId();
			Info.DisplayName = Zone->GetDisplayName();
			if (Snapshot && Snapshot->bMapReady)
			{
				if (const FASZoneRuntimeState* State = Snapshot->FindZone(Info.ZoneId))
				{
					Info.DangerLevel = State->DangerLevel;
				}
			}

			const TArray<FTransform> Transforms = bRespawn ? Zone->GetRespawnTransforms() : Zone->GetSpawnTransforms();
			Result.Reserve(Result.Num() + Transforms.Num());
			for (const FTransform& Transform : Transforms)
			{
				Info.Transform = Transform;
				Result.Add(Info);
			}
		};

		if (ZoneId.IsNone())
		{
			for (const AASMapZone* Zone : Subsystem->GetZones())
			{
				AppendZonePoints(Zone);
			}
		}
		else
		{
			AppendZonePoints(Subsystem->FindZone(ZoneId));
		}
		return Result;
	}
}

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

TArray<FASZoneSpawnPoint> UASMapFunctionLibrary::GetZoneSpawnPoints(const UObject* WorldContextObject, FName ZoneId)
{
	return ASMapFunctionLibraryPrivate::GetZonePoints(WorldContextObject, ZoneId, false);
}

TArray<FASZoneSpawnPoint> UASMapFunctionLibrary::GetZoneRespawnPoints(const UObject* WorldContextObject, FName ZoneId)
{
	return ASMapFunctionLibraryPrivate::GetZonePoints(WorldContextObject, ZoneId, true);
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
