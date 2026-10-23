#include "Mass/Monster/Processors/ASMassMonsterSuicideAttackProcessor.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Engine/World.h"
#include "Mass/Monster/Combat/ASMassMonsterDamageBridge.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Processors/ASMassMonsterBehaviorProcessor.h"
#include "Mass/Monster/Processors/ASMassMonsterNavigationProcessor.h"
#include "Mass/Monster/Subsystems/ASMonsterTargetSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterSuicideAttackProcessor)

namespace ASMassMonsterSuicide
{
	static void SpawnIndicator(UWorld& World, const FASMassMonsterConfigFragment& Config, const FVector& Origin)
	{
		if (!Config.SuicideIndicatorClass || Config.SuicideWindupDuration <= 0.0f)
		{
			return;
		}

		const FTransform SpawnTransform(FRotator::ZeroRotator, Origin);
		AASMonsterAttackIndicatorActor* Indicator = World.SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
			Config.SuicideIndicatorClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Indicator)
		{
			return;
		}

		Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Circle, Config.SuicideRadius,
			0.0f, 0.0f, Config.SuicideWindupDuration);
		Indicator->FinishSpawning(SpawnTransform);
		Indicator->SetLifeSpan(Config.SuicideWindupDuration);
	}
}

UASMassMonsterSuicideAttackProcessor::UASMassMonsterSuicideAttackProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteAfter.Add(UASMassMonsterBehaviorProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterNavigationProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterSuicideAttackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterRuntimeStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);
}

void UASMassMonsterSuicideAttackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterSuicideAttackProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterSuicideAttackProcessor);

	UWorld* World = GetWorld();
	UASMonsterTargetSubsystem* TargetSubsystem = World ? World->GetSubsystem<UASMonsterTargetSubsystem>() : nullptr;
	if (!World || !TargetSubsystem)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	TArray<FASTargetSnapshot> TargetSnapshots;
	TargetSubsystem->FindTargetsInRange(FVector::ZeroVector, TNumericLimits<float>::Max(),
		EASMonsterTargetCapability::PrimaryTarget, TargetSnapshots);
	EntityQuery.ForEachEntityChunk(Context, [World, CurrentTime, TargetSnapshots](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FASMassMonsterHealthFragment> Healths = Context.GetMutableFragmentView<FASMassMonsterHealthFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterTargetFragment> Targets = Context.GetFragmentView<FASMassMonsterTargetFragment>();
		const TArrayView<FASMassMonsterCombatFragment> Combats = Context.GetMutableFragmentView<FASMassMonsterCombatFragment>();
		const TConstArrayView<FASMassMonsterRuntimeStatsFragment> RuntimeStats = Context.GetFragmentView<FASMassMonsterRuntimeStatsFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		if (Config.AttackPattern != EASMassMonsterAttackPattern::Suicide)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterHealthFragment& Health = Healths[EntityIt];
			FASMassMonsterCombatFragment& Combat = Combats[EntityIt];
			if (Health.CurrentHealth <= 0.0f)
			{
				continue;
			}

			if (Combat.AttackState == EASMassMonsterAttackState::Ready)
			{
				if (Behaviors[EntityIt].State != EASMassMonsterState::Attack || !Targets[EntityIt].HasTarget())
				{
					continue;
				}

				Combat.PhaseEndTimeSeconds = CurrentTime + Config.SuicideWindupDuration;
				Combat.bPrimaryEffectApplied = false;
				Combat.AttackState = EASMassMonsterAttackState::Windup;
				ASMassMonsterSuicide::SpawnIndicator(*World, Config,
					Transforms[EntityIt].GetTransform().GetLocation());
				continue;
			}

			if (Combat.AttackState != EASMassMonsterAttackState::Windup || CurrentTime < Combat.PhaseEndTimeSeconds)
			{
				continue;
			}

			const FVector ExplosionOrigin = Transforms[EntityIt].GetTransform().GetLocation();
			for (const FASTargetSnapshot& Snapshot : TargetSnapshots)
			{
				if (!Snapshot.bAlive || !Snapshot.bAttackable || !Snapshot.Actor.IsValid()
					|| FVector::DistSquared2D(ExplosionOrigin, Snapshot.Location) > FMath::Square(Config.SuicideRadius))
				{
					continue;
				}

				ASMassMonsterDamage::ApplyToActor(Snapshot.Actor.Get(), RuntimeStats[EntityIt].AttackPower, ExplosionOrigin);
			}

			Combat.bPrimaryEffectApplied = true;
			Combat.NextAttackTimeSeconds = TNumericLimits<double>::Max();
			Combat.AttackState = EASMassMonsterAttackState::Recovering;
			Health.CurrentHealth = 0.0f;
		}
	});
}
