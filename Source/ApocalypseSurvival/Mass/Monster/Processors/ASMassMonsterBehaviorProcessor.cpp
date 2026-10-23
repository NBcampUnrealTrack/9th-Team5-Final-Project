#include "Mass/Monster/Processors/ASMassMonsterBehaviorProcessor.h"

#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Subsystems/ASMonsterTargetSubsystem.h"
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

	static bool CopySnapshotToTarget(const FASTargetSnapshot* Snapshot, FASMassMonsterTargetFragment& Target)
	{
		if (!Snapshot || !Snapshot->bAlive || !Snapshot->Actor.IsValid()
			|| !ASHasAllTargetCapabilities(Snapshot->Capabilities, EASMonsterTargetCapability::PrimaryTarget))
		{
			return false;
		}

		Target.PrimaryTarget = Snapshot->Handle;
		Target.CachedTargetLocation = Snapshot->Location;
		return true;
	}

	static void SetCombatApproachState(
		const FVector& EntityLocation,
		const FASMassMonsterConfigFragment& Config,
		const FASMassMonsterCombatFragment& Combat,
		const FASMassMonsterTargetFragment& Target,
		FASMassMonsterBehaviorFragment& Behavior)
	{
		const float DistanceSquared = FVector::DistSquared2D(EntityLocation, Target.CachedTargetLocation);
		// HitRadius는 근접 유닛의 중심점-대상 거리 보정용이다.
		// 원거리/돌진 유닛에 더하면 사거리보다 멀리서 Attack 상태가 되어 이동을 멈출 수 있다.
		const float EffectiveAttackRange = Combat.bUseFallbackMelee
			? Config.FallbackMeleeAttackRange
			: Config.AttackPattern == EASMassMonsterAttackPattern::Melee
				? Config.AttackRange + Config.HitRadius
				: Config.AttackRange;
		Behavior.State = DistanceSquared <= FMath::Square(EffectiveAttackRange)
			? EASMassMonsterState::Attack
			: EASMassMonsterState::Chase;
	}
}

UASMassMonsterBehaviorProcessor::UASMassMonsterBehaviorProcessor()
	: EntityQuery(*this)
{
	// 본맵과 기능 테스트 맵이 같은 행동 전환 경로를 쓰도록 자동 등록을 명시한다.
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterBehaviorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadOnly);
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
	UASMonsterTargetSubsystem* TargetSubsystem = World ? World->GetSubsystem<UASMonsterTargetSubsystem>() : nullptr;
	if (!TargetSubsystem)
	{
		return;
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context, [TargetSubsystem, DeltaTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FASMassMonsterHealthFragment> Healths = Context.GetMutableFragmentView<FASMassMonsterHealthFragment>();
		const TConstArrayView<FASMassMonsterCombatFragment> Combats = Context.GetFragmentView<FASMassMonsterCombatFragment>();
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
					FASTargetSnapshot Snapshot;
					const bool bHasCurrentSnapshot = Target.HasTarget()
						&& TargetSubsystem->ResolveTarget(Target.PrimaryTarget, Snapshot);

					if (!ASMassMonsterBehavior::CopySnapshotToTarget(
						bHasCurrentSnapshot ? &Snapshot : nullptr, Target))
					{
						const bool bFoundClosest = TargetSubsystem->FindClosestTargetInRange(
							EntityLocation, TNumericLimits<float>::Max(),
							EASMonsterTargetCapability::PrimaryTarget, Snapshot);
						if (!ASMassMonsterBehavior::CopySnapshotToTarget(bFoundClosest ? &Snapshot : nullptr, Target))
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

				ASMassMonsterBehavior::SetCombatApproachState(EntityLocation, Config, Combats[EntityIt], Target, Behavior);
				continue;
			}

			const float DistanceFromHomeSquared = FVector::DistSquared2D(EntityLocation, Home.HomeLocation);
			const bool bOutsideLeash = DistanceFromHomeSquared > FMath::Square(Home.LeashRadius);
			const bool bAtHome = DistanceFromHomeSquared <= FMath::Square(Home.ReturnAcceptanceRadius);

			if (Behavior.State == EASMassMonsterState::ReturnHome)
			{
				FASMassMonsterHealthFragment& Health = Healths[EntityIt];
				if (Home.ReturnHealthPercentPerSecond > 0.0f && Health.CurrentHealth < Config.MaxHealth)
				{
					Health.CurrentHealth = FMath::Min(Config.MaxHealth,
						Health.CurrentHealth + Config.MaxHealth * Home.ReturnHealthPercentPerSecond * DeltaTime);
				}

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
				FASTargetSnapshot Snapshot;
				const bool bHasSnapshot = bHadTarget
					? TargetSubsystem->ResolveTarget(Target.PrimaryTarget, Snapshot)
					: TargetSubsystem->FindClosestTargetInRange(EntityLocation, Home.DetectionRadius,
						EASMonsterTargetCapability::PrimaryTarget, Snapshot);
				const float ValidDistance = bHadTarget ? Home.LoseTargetRadius : Home.DetectionRadius;

				const bool bValidCurrentTarget = ASMassMonsterBehavior::CopySnapshotToTarget(
					bHasSnapshot ? &Snapshot : nullptr, Target)
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

			ASMassMonsterBehavior::SetCombatApproachState(EntityLocation, Config, Combats[EntityIt], Target, Behavior);
		}
	});
}
