#include "Mass/Monster/Processors/ASMassMonsterNavigationProcessor.h"

#include "ApocalypseSurvival.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Subsystems/ASMonsterTargetSubsystem.h"
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

	static void ResetProgressTracking(FASMassMonsterNavigationFragment& NavigationState)
	{
		NavigationState.LastProgressLocation = FVector::ZeroVector;
		NavigationState.StuckTimeSeconds = 0.0f;
		NavigationState.bHasProgressSample = false;
	}

	static bool IsMovementStalled(FASMassMonsterNavigationFragment& NavigationState,
		const FVector& CurrentLocation, const float DeltaTime,
		const FASMassMonsterConfigFragment& Config)
	{
		if (Config.BlockingTargetStuckSeconds <= 0.0f)
		{
			return false;
		}

		if (!NavigationState.bHasProgressSample)
		{
			NavigationState.LastProgressLocation = CurrentLocation;
			NavigationState.StuckTimeSeconds = 0.0f;
			NavigationState.bHasProgressSample = true;
			return false;
		}

		if (FVector::DistSquared2D(CurrentLocation, NavigationState.LastProgressLocation)
			>= FMath::Square(Config.BlockingTargetMinProgressDistance))
		{
			NavigationState.LastProgressLocation = CurrentLocation;
			NavigationState.StuckTimeSeconds = 0.0f;
			return false;
		}

		NavigationState.StuckTimeSeconds += FMath::Max(0.0f, DeltaTime);
		return NavigationState.StuckTimeSeconds >= Config.BlockingTargetStuckSeconds;
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

		FNavLocation ProjectedGoal;
		const FVector ProjectionExtent(
			FMath::Max(AgentRadius * 2.0f, 100.0f),
			FMath::Max(AgentRadius * 2.0f, 100.0f),
			FMath::Max(AgentHeight, 300.0f));
		if (!NavigationSystem->ProjectPointToNavigation(Goal, ProjectedGoal, ProjectionExtent, NavData))
		{
			return false;
		}

		FPathFindingQuery Query(NavigationSystem, *NavData, Start, ProjectedGoal.Location);
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

	static bool SelectBlockingTarget(const UASMonsterTargetSubsystem& TargetSubsystem,
		const FVector& EntityLocation, FASMassMonsterTargetFragment& Target,
		const FASMassMonsterConfigFragment& Config)
	{
		if (!Target.HasPrimaryTarget() || Target.HasBlockingTarget() || Config.BlockingTargetSearchRadius <= 0.0f)
		{
			return false;
		}

		const FVector PrimaryOffset = Target.CachedPrimaryTargetLocation - EntityLocation;
		const float PrimaryDistance = PrimaryOffset.Size2D();
		if (PrimaryDistance <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const FVector PrimaryDirection = PrimaryOffset.GetSafeNormal2D();
		TArray<FASTargetSnapshot> Candidates;
		TargetSubsystem.FindTargetsInRange(EntityLocation, Config.BlockingTargetSearchRadius,
			EASMonsterTargetCapability::BlockingTarget, Candidates);

		const FASTargetSnapshot* BestCandidate = nullptr;
		float BestScore = TNumericLimits<float>::Max();
		for (const FASTargetSnapshot& Candidate : Candidates)
		{
			if (!Candidate.bAlive || !Candidate.bAttackable || !Candidate.Actor.IsValid())
			{
				continue;
			}

			const FVector CandidateOffset = Candidate.Location - EntityLocation;
			const float CandidateDistance = CandidateOffset.Size2D();
			if (CandidateDistance <= KINDA_SMALL_NUMBER || CandidateDistance >= PrimaryDistance)
			{
				continue;
			}

			const FVector CandidateDirection = CandidateOffset.GetSafeNormal2D();
			const float ForwardDot = FVector::DotProduct(PrimaryDirection, CandidateDirection);
			if (ForwardDot < Config.BlockingTargetMinForwardDot)
			{
				continue;
			}

			const float AlongPrimary = FVector::DotProduct(CandidateOffset, PrimaryDirection);
			const float LateralDistance = FMath::Sqrt(FMath::Max(0.0f,
				CandidateOffset.SizeSquared2D() - FMath::Square(AlongPrimary)));
			const float Score = LateralDistance + CandidateDistance * 0.1f;
			if (!BestCandidate || Score < BestScore
				|| (FMath::IsNearlyEqual(Score, BestScore) && Candidate.Handle.Id < BestCandidate->Handle.Id))
			{
				BestCandidate = &Candidate;
				BestScore = Score;
			}
		}

		if (!BestCandidate)
		{
			return false;
		}

		Target.BlockingTarget = BestCandidate->Handle;
		Target.CachedBlockingTargetLocation = BestCandidate->Location;
		return true;
	}
}

UASMassMonsterNavigationProcessor::UASMassMonsterNavigationProcessor()
	: EntityQuery(*this)
{
	// 본맵과 기능 테스트 맵이 같은 경로 이동 Processor를 실행하도록 자동 등록을 명시한다.
	bAutoRegisterWithProcessingPhases = true;
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
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterModifierFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterRuntimeStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadWrite);
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
	UASMonsterTargetSubsystem* TargetSubsystem = World ? World->GetSubsystem<UASMonsterTargetSubsystem>() : nullptr;
	if (!World || !TargetSubsystem)
	{
		return;
	}

	int32 RemainingPathRequests = FMath::Max(1, MaxPathRequestsPerFrame);
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [this, World, TargetSubsystem, DeltaTime, &RemainingPathRequests](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> AgentRadii = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FAgentHeightFragment> AgentHeights = Context.GetFragmentView<FAgentHeightFragment>();
		const TConstArrayView<FMassDesiredMovementFragment> DesiredMovements = Context.GetFragmentView<FMassDesiredMovementFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterCombatFragment> Combats = Context.GetFragmentView<FASMassMonsterCombatFragment>();
		const TConstArrayView<FASMassMonsterModifierFragment> Modifiers = Context.GetFragmentView<FASMassMonsterModifierFragment>();
		const TConstArrayView<FASMassMonsterRuntimeStatsFragment> RuntimeStats = Context.GetFragmentView<FASMassMonsterRuntimeStatsFragment>();
		const TArrayView<FASMassMonsterTargetFragment> Targets = Context.GetMutableFragmentView<FASMassMonsterTargetFragment>();
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
			const FASMassMonsterCombatFragment& Combat = Combats[EntityIt];

			// 돌진 준비/실행과 자폭 시전 중에는 행동 상태가 Chase로 바뀌어도 이동을 재개하지 않는다.
			const bool bLocksMovementForPattern =
				(Config.AttackPattern == EASMassMonsterAttackPattern::Charge
					&& (Combat.AttackState == EASMassMonsterAttackState::Windup
						|| Combat.AttackState == EASMassMonsterAttackState::Executing))
				|| (Config.AttackPattern == EASMassMonsterAttackPattern::Suicide
					&& Combat.AttackState == EASMassMonsterAttackState::Windup);
			if (bLocksMovementForPattern)
			{
				ASMassMonsterNavigation::Stop(*World, MoveTarget, CachedPath, ShortPath);
				ASMassMonsterNavigation::ResetProgressTracking(NavigationState);
				NavigationState.bHasRequestedGoal = false;
				NavigationState.PathRefreshTimeRemaining = 0.0f;
				continue;
			}

			FVector Goal = FVector::ZeroVector;
			float EndDistanceThreshold = 20.0f;
			bool bShouldMove = false;

			FASMassMonsterTargetFragment& Target = Targets[EntityIt];
			if (Behavior.State == EASMassMonsterState::Chase && Target.HasTarget())
			{
				Goal = Target.GetActiveTargetLocation();
				// BehaviorProcessor와 동일한 접근 거리 규칙을 사용해야 원거리 유닛이
				// HitRadius만큼 이른 Attack 상태로 전환되어 멈추는 일이 없다.
				const float EffectiveAttackRange = Combats[EntityIt].bUseFallbackMelee
					? Config.FallbackMeleeAttackRange
					: Config.AttackPattern == EASMassMonsterAttackPattern::Melee
						? Config.AttackRange + Config.HitRadius
						: Config.AttackRange;
				EndDistanceThreshold = FMath::Max(20.0f, EffectiveAttackRange);
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
				ASMassMonsterNavigation::ResetProgressTracking(NavigationState);
				NavigationState.bHasRequestedGoal = false;
				NavigationState.PathRefreshTimeRemaining = 0.0f;
				continue;
			}

			// 버프 지속 중에는 이미 잡힌 경로를 다시 요청하지 않아도 속도를 즉시 반영한다.
			// 라운드 성장으로 확정한 기본 속도에 일시 버프 배율만 추가한다.
			const float EffectiveMoveSpeed = RuntimeStats[EntityIt].MoveSpeed * FMath::Max(1.0f, Modifiers[EntityIt].MoveSpeedMultiplier);
			MoveTarget.DesiredSpeed.Set(FMath::Min3(
				EffectiveMoveSpeed,
				DesiredMovements[EntityIt].DesiredMaxSpeedOverride,
				MovementParameters.MaxSpeed));

			NavigationState.PathRefreshTimeRemaining = FMath::Max(0.0f, NavigationState.PathRefreshTimeRemaining - DeltaTime);
			const FVector EntityLocation = Transforms[EntityIt].GetTransform().GetLocation();
			const bool bProgressStalled = Behavior.State == EASMassMonsterState::Chase
				&& Target.HasPrimaryTarget() && !Target.HasBlockingTarget()
				&& ASMassMonsterNavigation::IsMovementStalled(
					NavigationState, EntityLocation, DeltaTime, Config);

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
			if (bProgressStalled && bRefreshDue
				&& ASMassMonsterNavigation::SelectBlockingTarget(*TargetSubsystem, EntityLocation, Target, Config))
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(LogApocalypseSurvival, Log,
					TEXT("[MassMonsterBlocking] Entity=%d Primary=%d:%d Blocking=%d:%d Reason=Stuck"),
					Context.GetEntity(EntityIt).Index, Target.PrimaryTarget.Id, Target.PrimaryTarget.Generation,
					Target.BlockingTarget.Id, Target.BlockingTarget.Generation);
#endif
				ASMassMonsterNavigation::Stop(*World, MoveTarget, CachedPath, ShortPath);
				ASMassMonsterNavigation::ResetProgressTracking(NavigationState);
				NavigationState.bHasRequestedGoal = false;
				NavigationState.PathRefreshTimeRemaining = 0.0f;
				continue;
			}

			if (RemainingPathRequests <= 0 || !bRefreshDue || (!bGoalMoved && !bPathUnavailable))
			{
				continue;
			}

			--RemainingPathRequests;
			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
			float RequestedSpeed = FMath::Min3(
				EffectiveMoveSpeed,
				DesiredMovements[EntityIt].DesiredMaxSpeedOverride,
				MovementParameters.MaxSpeed);
			if (Behavior.State == EASMassMonsterState::ReturnHome)
			{
				RequestedSpeed *= FMath::Max(1.0f, Homes[EntityIt].ReturnMoveSpeedMultiplier);
			}
			const bool bPathRequested = ASMassMonsterNavigation::RequestPath(
				*World,
				EntityLocation,
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
				if (!Target.HasBlockingTarget()
					&& ASMassMonsterNavigation::SelectBlockingTarget(*TargetSubsystem,
						Transforms[EntityIt].GetTransform().GetLocation(), Target, Config))
				{
#if !UE_BUILD_SHIPPING
					UE_LOG(LogApocalypseSurvival, Log,
						TEXT("[MassMonsterBlocking] Entity=%d Primary=%d:%d Blocking=%d:%d Reason=PathFailure"),
						Entity.Index, Target.PrimaryTarget.Id, Target.PrimaryTarget.Generation,
						Target.BlockingTarget.Id, Target.BlockingTarget.Generation);
#endif
					ASMassMonsterNavigation::Stop(*World, MoveTarget, CachedPath, ShortPath);
					ASMassMonsterNavigation::ResetProgressTracking(NavigationState);
					NavigationState.bHasRequestedGoal = false;
					NavigationState.PathRefreshTimeRemaining = 0.0f;
					continue;
				}
				if (Target.HasBlockingTarget())
				{
					Target.ClearBlockingTarget();
				}
				ASMassMonsterNavigation::Stop(*World, MoveTarget, CachedPath, ShortPath);
				ASMassMonsterNavigation::ResetProgressTracking(NavigationState);
			}

			NavigationState.LastRequestedGoal = Goal;
			NavigationState.bHasRequestedGoal = true;
			NavigationState.PathRefreshTimeRemaining = ASMassMonsterNavigation::MakeRefreshInterval(Config.PathRefreshInterval, Entity);
		}
	});
}
