#include "Mass/Monster/Processors/ASMassMonsterNavigationProcessor.h"

#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "MassNavMeshNavigationProcessors.h"
#include "NavCorridor.h"
#include "NavigationSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterNavigationProcessor)

namespace ASMassMonsterNavigation
{
	static void ResetCachedPath(FMassNavMeshCachedPathFragment& CachedPath, FMassNavMeshShortPathFragment& ShortPath)
	{
		CachedPath.NavPath.Reset();
		CachedPath.Corridor.Reset();
		CachedPath.NavPathNextStartIndex = 0;
		CachedPath.PathSource = EMassNavigationPathSource::Unset;
		ShortPath.Reset();
	}

	static void Stop(const UWorld& World, FMassMoveTargetFragment& MoveTarget,
		FMassNavMeshCachedPathFragment& CachedPath, FMassNavMeshShortPathFragment& ShortPath)
	{
		if (MoveTarget.GetCurrentAction() != EMassMovementAction::Stand)
		{
			MoveTarget.CreateNewAction(EMassMovementAction::Stand, World);
		}

		MoveTarget.DesiredSpeed.Set(0.0f);
		MoveTarget.DistanceToGoal = 0.0f;
		MoveTarget.EntityDistanceToGoal = FMassMoveTargetFragment::UnsetDistance;
		MoveTarget.IntentAtGoal = EMassMovementAction::Stand;
		ResetCachedPath(CachedPath, ShortPath);
	}

	static bool UpdateShortPath(FMassNavMeshCachedPathFragment& CachedPath, FMassNavMeshShortPathFragment& ShortPath,
		const float EndDistanceThreshold)
	{
		if (!CachedPath.Corridor || !ShortPath.bPartialResult)
		{
			return false;
		}

		const bool bRequested = ShortPath.RequestShortPath(
			CachedPath.Corridor,
			CachedPath.NavPathNextStartIndex,
			FMassNavMeshCachedPathFragment::NumLeadingPoints,
			EndDistanceThreshold);

		if (bRequested)
		{
			CachedPath.NavPathNextStartIndex += static_cast<uint16>(FMath::Max(
				ShortPath.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate
					- FMassNavMeshCachedPathFragment::NumLeadingPoints,
				0));
		}

		return bRequested;
	}

	static bool RequestPath(
		UWorld& World,
		const FVector& Start,
		const FVector& Goal,
		const float AgentRadius,
		const float AgentHeight,
		const float DesiredSpeed,
		const float EndDistanceThreshold,
		const float CorridorWidth,
		const float OffsetFromBoundaries,
		FMassMoveTargetFragment& MoveTarget,
		FMassNavMeshCachedPathFragment& CachedPath,
		FMassNavMeshShortPathFragment& ShortPath)
	{
		UNavigationSystemV1* NavigationSystem = Cast<UNavigationSystemV1>(World.GetNavigationSystem());
		if (!NavigationSystem)
		{
			return false;
		}

		const FNavAgentProperties NavAgentProperties(AgentRadius, AgentHeight);
		const ANavigationData* NavData = NavigationSystem->GetNavDataForProps(NavAgentProperties, Start);
		if (!NavData)
		{
			return false;
		}

		FPathFindingQuery Query(NavigationSystem, *NavData, Start, Goal);
		FPathFindingResult Result = NavData->FindPath(NavAgentProperties, Query);
		if (!Result.IsSuccessful() || !Result.Path.IsValid())
		{
			return false;
		}

		Result.Path->RemoveOverlappingPoints(FNavCorridor::OverlappingPointTolerance);
		if (Result.Path->GetPathPoints().Num() <= 1)
		{
			return false;
		}

		TSharedPtr<FNavCorridor> NewCorridor = MakeShared<FNavCorridor>();
		const FSharedConstNavQueryFilter QueryFilter = Query.QueryFilter ? Query.QueryFilter : NavData->GetDefaultQueryFilter();
		FNavCorridorParams CorridorParams;
		CorridorParams.SetFromWidth(CorridorWidth);
		CorridorParams.PathOffsetFromBoundaries = OffsetFromBoundaries;
		NewCorridor->BuildFromPath(*Result.Path, QueryFilter, CorridorParams);

		FMassNavMeshShortPathFragment NewShortPath;
		if (!NewShortPath.RequestShortPath(NewCorridor, 0, 0, EndDistanceThreshold))
		{
			return false;
		}

		CachedPath.NavPath = Result.Path;
		CachedPath.Corridor = MoveTemp(NewCorridor);
		CachedPath.PathSource = EMassNavigationPathSource::NavMesh;
		CachedPath.NavPathNextStartIndex = static_cast<uint16>(FMath::Max(
			NewShortPath.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate
				- FMassNavMeshCachedPathFragment::NumLeadingPoints,
			0));
		ShortPath = MoveTemp(NewShortPath);

		MoveTarget.DesiredSpeed.Set(DesiredSpeed);
		MoveTarget.IntentAtGoal = EMassMovementAction::Stand;
		MoveTarget.CreateNewAction(EMassMovementAction::Move, World);
		return true;
	}

	static float MakeRefreshInterval(const float BaseInterval, const FMassEntityHandle Entity)
	{
		const uint32 Bucket = GetTypeHash(Entity.Index) % 31u;
		const float Scale = 0.85f + (static_cast<float>(Bucket) / 30.0f) * 0.3f;
		return FMath::Max(0.01f, BaseInterval * Scale);
	}
}

UASMassMonsterNavigationProcessor::UASMassMonsterNavigationProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteBefore.Add(UMassNavMeshPathFollowProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterNavigationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentHeightFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavMeshCachedPathFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavMeshShortPathFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterNavigationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
}

void UASMassMonsterNavigationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterNavigationProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterNavigationProcessor);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 RemainingPathRequests = FMath::Max(1, MaxPathRequestsPerFrame);
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [this, World, DeltaTime, &RemainingPathRequests](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> AgentRadii = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FAgentHeightFragment> AgentHeights = Context.GetFragmentView<FAgentHeightFragment>();
		const TConstArrayView<FMassDesiredMovementFragment> DesiredMovements = Context.GetFragmentView<FMassDesiredMovementFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterTargetFragment> Targets = Context.GetFragmentView<FASMassMonsterTargetFragment>();
		const TConstArrayView<FASMassMonsterHomeFragment> Homes = Context.GetFragmentView<FASMassMonsterHomeFragment>();

		const TArrayView<FMassMoveTargetFragment> MoveTargets = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassNavMeshCachedPathFragment> CachedPaths = Context.GetMutableFragmentView<FMassNavMeshCachedPathFragment>();
		const TArrayView<FMassNavMeshShortPathFragment> ShortPaths = Context.GetMutableFragmentView<FMassNavMeshShortPathFragment>();
		const TArrayView<FASMassMonsterNavigationFragment> NavigationStates = Context.GetMutableFragmentView<FASMassMonsterNavigationFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();
		const FMassMovementParameters& MovementParameters = Context.GetConstSharedFragment<FMassMovementParameters>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassMoveTargetFragment& MoveTarget = MoveTargets[EntityIt];
			FMassNavMeshCachedPathFragment& CachedPath = CachedPaths[EntityIt];
			FMassNavMeshShortPathFragment& ShortPath = ShortPaths[EntityIt];
			FASMassMonsterNavigationFragment& NavigationState = NavigationStates[EntityIt];
			const FASMassMonsterBehaviorFragment& Behavior = Behaviors[EntityIt];

			FVector Goal = FVector::ZeroVector;
			float EndDistanceThreshold = 20.0f;
			bool bShouldMove = false;

			if (Behavior.State == EASMassMonsterState::Chase && Targets[EntityIt].HasTarget())
			{
				Goal = Targets[EntityIt].CachedTargetLocation;
				EndDistanceThreshold = FMath::Max(20.0f, Config.AttackRange);
				bShouldMove = true;
			}
			else if (Behavior.State == EASMassMonsterState::ReturnHome)
			{
				Goal = Homes[EntityIt].HomeLocation;
				EndDistanceThreshold = Homes[EntityIt].ReturnAcceptanceRadius;
				bShouldMove = true;
			}

			if (!bShouldMove)
			{
				ASMassMonsterNavigation::Stop(*World, MoveTarget, CachedPath, ShortPath);
				NavigationState.bHasRequestedGoal = false;
				NavigationState.PathRefreshTimeRemaining = 0.0f;
				continue;
			}

			NavigationState.PathRefreshTimeRemaining = FMath::Max(0.0f, NavigationState.PathRefreshTimeRemaining - DeltaTime);

			if (ShortPath.IsDone() && ShortPath.bPartialResult
				&& ASMassMonsterNavigation::UpdateShortPath(CachedPath, ShortPath, EndDistanceThreshold))
			{
				continue;
			}

			const float RepathDistanceSquared = FMath::Square(FMath::Max(1.0f, Config.TargetMoveRepathDistance));
			const bool bGoalMoved = !NavigationState.bHasRequestedGoal
				|| FVector::DistSquared2D(Goal, NavigationState.LastRequestedGoal) > RepathDistanceSquared;
			const bool bPathUnavailable = !CachedPath.NavPath.IsValid() || ShortPath.IsDone();
			const bool bRefreshDue = NavigationState.PathRefreshTimeRemaining <= 0.0f;

			if (RemainingPathRequests <= 0 || !bRefreshDue || (!bGoalMoved && !bPathUnavailable))
			{
				continue;
			}

			--RemainingPathRequests;
			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
			const float RequestedSpeed = FMath::Min3(
				Config.MoveSpeed,
				DesiredMovements[EntityIt].DesiredMaxSpeedOverride,
				MovementParameters.MaxSpeed);
			const bool bPathRequested = ASMassMonsterNavigation::RequestPath(
				*World,
				Transforms[EntityIt].GetTransform().GetLocation(),
				Goal,
				AgentRadii[EntityIt].Radius,
				AgentHeights[EntityIt].Height,
				RequestedSpeed,
				EndDistanceThreshold,
				CorridorWidth,
				OffsetFromBoundaries,
				MoveTarget,
				CachedPath,
				ShortPath);
			if (!bPathRequested && !CachedPath.NavPath.IsValid())
			{
				ASMassMonsterNavigation::Stop(*World, MoveTarget, CachedPath, ShortPath);
			}

			NavigationState.LastRequestedGoal = Goal;
			NavigationState.bHasRequestedGoal = true;
			NavigationState.PathRefreshTimeRemaining = ASMassMonsterNavigation::MakeRefreshInterval(Config.PathRefreshInterval, Entity);
		}
	});
}
