#include "Mass/Monster/Processors/ASMassMonsterBehaviorProcessor.h"

#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Subsystems/ASMassPlayerTargetSubsystem.h"
#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterBehaviorProcessor)

namespace ASMassMonsterBehavior
{
	static float MakeRefreshInterval(const float BaseInterval, const FMassEntityHandle Entity)
	{
		const uint32 Bucket = GetTypeHash(Entity.Index) % 31u;
		const float Scale = 0.85f + (static_cast<float>(Bucket) / 30.0f) * 0.3f;
		return FMath::Max(0.01f, BaseInterval * Scale);
	}

	static float MakeInitialRefreshDelay(const float BaseInterval, const FMassEntityHandle Entity)
	{
		const uint32 Bucket = GetTypeHash(Entity.Index) % 32u;
		return FMath::Max(0.0f, BaseInterval * (static_cast<float>(Bucket) / 31.0f));
	}

	static void ClearTarget(FASMassMonsterTargetFragment& Target, const float NextRefreshDelay)
	{
		Target.Reset();
		Target.bRefreshScheduleInitialized = true;
		Target.RefreshTimeRemaining = FMath::Max(0.01f, NextRefreshDelay);
	}

	static bool CopySnapshotToTarget(const FASMassPlayerTargetSnapshot* Snapshot, FASMassMonsterTargetFragment& Target)
	{
		if (!Snapshot || !Snapshot->bAlive || !Snapshot->Pawn.IsValid())
		{
			return false;
		}

		Target.TargetId = Snapshot->TargetId;
		Target.CachedTargetLocation = Snapshot->Location;
		return true;
	}

	static void SetCombatApproachState(
		const FVector& EntityLocation,
		const FASMassMonsterConfigFragment& Config,
		const FASMassMonsterTargetFragment& Target,
		FASMassMonsterBehaviorFragment& Behavior)
	{
		const float DistanceSquared = FVector::DistSquared2D(EntityLocation, Target.CachedTargetLocation);
		Behavior.State = DistanceSquared <= FMath::Square(Config.AttackRange)
			? EASMassMonsterState::Attack
			: EASMassMonsterState::Chase;
	}
}

UASMassMonsterBehaviorProcessor::UASMassMonsterBehaviorProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterBehaviorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
}

void UASMassMonsterBehaviorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterBehaviorProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterBehaviorProcessor);

	UWorld* World = GetWorld();
	UASMassPlayerTargetSubsystem* TargetSubsystem = World ? World->GetSubsystem<UASMassPlayerTargetSubsystem>() : nullptr;
	if (!TargetSubsystem)
	{
		return;
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context, [TargetSubsystem, DeltaTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FASMassMonsterHealthFragment> Healths = Context.GetFragmentView<FASMassMonsterHealthFragment>();
		const TConstArrayView<FASMassMonsterHomeFragment> Homes = Context.GetFragmentView<FASMassMonsterHomeFragment>();
		const TArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetMutableFragmentView<FASMassMonsterBehaviorFragment>();
		const TArrayView<FASMassMonsterTargetFragment> Targets = Context.GetMutableFragmentView<FASMassMonsterTargetFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterBehaviorFragment& Behavior = Behaviors[EntityIt];
			FASMassMonsterTargetFragment& Target = Targets[EntityIt];
			const FASMassMonsterHomeFragment& Home = Homes[EntityIt];
			const FVector EntityLocation = Transforms[EntityIt].GetTransform().GetLocation();
			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);

			if (Healths[EntityIt].CurrentHealth <= 0.0f)
			{
				Behavior.State = EASMassMonsterState::Dead;
				Target.Reset();
				continue;
			}

			Target.Aggro = FMath::Max(0.0f, Target.Aggro - Config.AggroDecayPerSecond * DeltaTime);
			Target.RefreshTimeRemaining = FMath::Max(0.0f, Target.RefreshTimeRemaining - DeltaTime);
			if (!Target.bRefreshScheduleInitialized)
			{
				Target.bRefreshScheduleInitialized = true;
				Target.RefreshTimeRemaining = ASMassMonsterBehavior::MakeInitialRefreshDelay(Config.TargetRefreshInterval, Entity);
			}

			const float RefreshInterval = ASMassMonsterBehavior::MakeRefreshInterval(Config.TargetRefreshInterval, Entity);
			const bool bRefreshTarget = Target.RefreshTimeRemaining <= 0.0f;
			if (bRefreshTarget)
			{
				Target.RefreshTimeRemaining = RefreshInterval;
			}

			if (Behavior.WorldBehavior == EASMassMonsterWorldBehavior::NightAssault)
			{
				if (bRefreshTarget)
				{
					const FASMassPlayerTargetSnapshot* Snapshot = Target.HasTarget()
						? TargetSubsystem->FindTarget(Target.TargetId)
						: nullptr;

					if (!ASMassMonsterBehavior::CopySnapshotToTarget(Snapshot, Target))
					{
						Snapshot = TargetSubsystem->FindClosestAliveTarget(EntityLocation, TNumericLimits<float>::Max());
						if (!ASMassMonsterBehavior::CopySnapshotToTarget(Snapshot, Target))
						{
							ASMassMonsterBehavior::ClearTarget(Target, RefreshInterval);
						}
					}
				}

				if (!Target.HasTarget())
				{
					Behavior.State = EASMassMonsterState::Idle;
					continue;
				}

				ASMassMonsterBehavior::SetCombatApproachState(EntityLocation, Config, Target, Behavior);
				continue;
			}

			const float DistanceFromHomeSquared = FVector::DistSquared2D(EntityLocation, Home.HomeLocation);
			const bool bOutsideLeash = DistanceFromHomeSquared > FMath::Square(Home.LeashRadius);
			const bool bAtHome = DistanceFromHomeSquared <= FMath::Square(Home.ReturnAcceptanceRadius);

			if (Behavior.State == EASMassMonsterState::ReturnHome)
			{
				if (bAtHome)
				{
					Behavior.State = EASMassMonsterState::Idle;
				}
				Target.Reset();
				Target.bRefreshScheduleInitialized = true;
				continue;
			}

			if (bOutsideLeash)
			{
				Behavior.State = EASMassMonsterState::ReturnHome;
				ASMassMonsterBehavior::ClearTarget(Target, RefreshInterval);
				continue;
			}

			if (bRefreshTarget)
			{
				const bool bHadTarget = Target.HasTarget();
				const FASMassPlayerTargetSnapshot* Snapshot = bHadTarget
					? TargetSubsystem->FindTarget(Target.TargetId)
					: TargetSubsystem->FindClosestAliveTarget(EntityLocation, Config.DetectionRadius);
				const float ValidDistance = bHadTarget ? Config.LoseTargetRadius : Config.DetectionRadius;

				const bool bValidCurrentTarget = ASMassMonsterBehavior::CopySnapshotToTarget(Snapshot, Target)
					&& FVector::DistSquared2D(EntityLocation, Target.CachedTargetLocation)
						<= FMath::Square(ValidDistance);

				if (!bValidCurrentTarget)
				{
					ASMassMonsterBehavior::ClearTarget(Target, RefreshInterval);
				}
			}

			if (!Target.HasTarget())
			{
				Behavior.State = bAtHome ? EASMassMonsterState::Idle : EASMassMonsterState::ReturnHome;
				continue;
			}

			ASMassMonsterBehavior::SetCombatApproachState(EntityLocation, Config, Target, Behavior);
		}
	});
}
