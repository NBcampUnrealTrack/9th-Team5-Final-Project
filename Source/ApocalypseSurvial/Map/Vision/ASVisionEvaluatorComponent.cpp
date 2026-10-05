#include "Map/Vision/ASVisionEvaluatorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Map/Core/ASMapDefinition.h"
#include "Map/Core/ASMapFunctionLibrary.h"
#include "Map/Core/ASMapStateComponent.h"
#include "Map/Vision/ASVisibilityComponent.h"
#include "Map/Vision/ASVisionSourceComponent.h"
#include "Map/Vision/ASVisionWorldSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UASVisionEvaluatorComponent::UASVisionEvaluatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UASVisionEvaluatorComponent::Configure(UASMapDefinition* Definition)
{
	MapDefinition = Definition;
}

bool UASVisionEvaluatorComponent::CanSourceSeeTarget(
	const UASVisionSourceComponent* Source,
	const UASVisibilityComponent* Target,
	double ServerTime,
	bool bWallOcclusion) const
{
	const AActor* SourceActor = Source ? Source->GetOwner() : nullptr;
	const AActor* TargetActor = Target ? Target->GetOwner() : nullptr;
	if (!SourceActor || !TargetActor || !MapDefinition)
	{
		return false;
	}

	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const float Radius = Source->GetVisionRadius(MapDefinition->Vision.DefaultVisionRadius);

	if (FVector::DistSquared2D(SourceLocation, TargetLocation) > FMath::Square(Radius))
	{
		return false;
	}

	// 수풀 은폐는 벽 옵션과 독립적이며 공격 노출로만 해제된다.
	if (const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		const FName TargetBush = Subsystem->FindBushIdAtLocation(TargetLocation);
		const bool bRevealed = Target->GetRevealEndServerTime() > ServerTime;
		if (!TargetBush.IsNone() && !bRevealed && Subsystem->FindBushIdAtLocation(SourceLocation) != TargetBush)
		{
			return false;
		}
	}

	// 노출은 벽이나 거리 조건을 우회하지 않는다.
	if (bWallOcclusion && IsBlockedByWall(GetTraceLocation(SourceActor), GetTraceLocation(TargetActor), SourceActor, TargetActor))
	{
		return false;
	}

	return true;
}

void UASVisionEvaluatorComponent::ServerEvaluateVisibility()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASFogOfWar_ServerEvaluate);

	const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this);
	const UASMapStateComponent* MapState = UASMapFunctionLibrary::GetMapStateComponent(this);
	const UWorld* World = GetWorld();
	if (!Subsystem || !MapState || !World)
	{
		return;
	}

	const bool bWallOcclusion = MapState->IsWallOcclusionEnabled();
	const double Now = MapState->GetServerTimeSeconds();

	TMap<int32, TArray<UASVisionSourceComponent*>> SourcesByTeam;
	for (UASVisionSourceComponent* Source : Subsystem->GetVisionSources())
	{
		if (Source->IsProvidingVision(MapDefinition->DeadStateTag))
		{
			SourcesByTeam.FindOrAdd(Source->GetTeamId()).Add(Source);
		}
	}

	for (UASVisibilityComponent* Target : Subsystem->GetVisibilityTargets())
	{
		if (Target->IsAlwaysVisible())
		{
			Target->ServerSetVisibleTeamMask(0xFF);
			continue;
		}

		uint8 Mask = 0;
		for (const TPair<int32, TArray<UASVisionSourceComponent*>>& Team : SourcesByTeam)
		{
			if (Team.Key < 0 || Team.Key > 7)
			{
				continue;
			}

			for (const UASVisionSourceComponent* Source : Team.Value)
			{
				if (Source->GetOwner() == Target->GetOwner() || CanSourceSeeTarget(Source, Target, Now, bWallOcclusion))
				{
					Mask |= static_cast<uint8>(1 << Team.Key);
					break;
				}
			}
		}

		Target->ServerSetVisibleTeamMask(Mask);
	}
}

bool UASVisionEvaluatorComponent::IsBlockedByWall(const FVector& From, const FVector& To, const AActor* IgnoreA, const AActor* IgnoreB) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASFogOfWarVisibility), false);
	Params.AddIgnoredActor(IgnoreA);
	Params.AddIgnoredActor(IgnoreB);
	return GetWorld()->LineTraceTestByChannel(From, To, VisionWallChannel, Params);
}

FVector UASVisionEvaluatorComponent::GetTraceLocation(const AActor* Actor) const
{
	const float Offset = MapDefinition ? MapDefinition->Vision.TraceHeightOffset : 50.0f;
	return Actor ? Actor->GetActorLocation() + FVector(0.0, 0.0, Offset) : FVector::ZeroVector;
}

bool UASVisionEvaluatorComponent::TraceVisionWall(FHitResult& Hit, const FVector& From, const FVector& To, const AActor* IgnoreActor) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASFogOfWarLocalMask), false, IgnoreActor);
	return GetWorld()->LineTraceSingleByChannel(Hit, From, To, VisionWallChannel, Params);
}
