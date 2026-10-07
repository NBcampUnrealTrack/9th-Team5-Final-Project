#include "MapSystem/Vision/ASVisionEvaluatorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisionDefinition.h"
#include "MapSystem/Vision/ASVisibilityComponent.h"
#include "MapSystem/Vision/ASVisionSourceComponent.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UASVisionEvaluatorComponent::UASVisionEvaluatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UASVisionEvaluatorComponent::Configure(UASVisionDefinition* Definition)
{
	VisionDefinition = Definition;
}

bool UASVisionEvaluatorComponent::CanSourceSeeTarget(
	const UASVisionSourceComponent* Source,
	const UASVisibilityComponent* Target,
	double ServerTime,
	bool bWallOcclusion) const
{
	const AActor* SourceActor = Source ? Source->GetOwner() : nullptr;
	const AActor* TargetActor = Target ? Target->GetOwner() : nullptr;
	if (!SourceActor || !TargetActor || !VisionDefinition)
	{
		return false;
	}

	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const float Radius = Source->GetVisionRadius(VisionDefinition->Settings.DefaultVisionRadius);

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
	const AASFogOfWarManager* Manager = Cast<AASFogOfWarManager>(GetOwner());
	const UWorld* World = GetWorld();
	if (!Subsystem || !Manager || !Manager->HasAuthority() || !VisionDefinition || !World)
	{
		return;
	}

	const bool bWallOcclusion = Manager->IsWallOcclusionEnabled();
	const double Now = GetServerTimeSeconds();

	TMap<int32, TArray<UASVisionSourceComponent*>> SourcesByTeam;
	for (UASVisionSourceComponent* Source : Subsystem->GetVisionSources())
	{
		if (Source->IsProvidingVision(VisionDefinition->DeadStateTag))
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
	const float Offset = VisionDefinition ? VisionDefinition->Settings.TraceHeightOffset : 50.0f;
	return Actor ? Actor->GetActorLocation() + FVector(0.0, 0.0, Offset) : FVector::ZeroVector;
}

bool UASVisionEvaluatorComponent::TraceVisionWall(FHitResult& Hit, const FVector& From, const FVector& To, const AActor* IgnoreActor) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASFogOfWarLocalMask), false, IgnoreActor);
	return GetWorld()->LineTraceSingleByChannel(Hit, From, To, VisionWallChannel, Params);
}

double UASVisionEvaluatorComponent::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}
