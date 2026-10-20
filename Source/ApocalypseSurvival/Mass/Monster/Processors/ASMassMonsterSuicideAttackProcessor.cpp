#include "Mass/Monster/Processors/ASMassMonsterSuicideAttackProcessor.h"

#include "Engine/World.h"
#include "Mass/Monster/Combat/ASMassMonsterDamageBridge.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Subsystems/ASMassPlayerTargetSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterSuicideAttackProcessor)

UASMassMonsterSuicideAttackProcessor::UASMassMonsterSuicideAttackProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterSuicideAttackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);
}

void UASMassMonsterSuicideAttackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterSuicideAttackProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterSuicideAttackProcessor);

	UWorld* World = GetWorld();
	UASMassPlayerTargetSubsystem* TargetSubsystem = World ? World->GetSubsystem<UASMassPlayerTargetSubsystem>() : nullptr;
	if (!World || !TargetSubsystem)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	const TConstArrayView<FASMassPlayerTargetSnapshot> TargetSnapshots = TargetSubsystem->GetTargetSnapshots();
	EntityQuery.ForEachEntityChunk(Context, [CurrentTime, TargetSnapshots](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FASMassMonsterHealthFragment> Healths = Context.GetMutableFragmentView<FASMassMonsterHealthFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterTargetFragment> Targets = Context.GetFragmentView<FASMassMonsterTargetFragment>();
		const TArrayView<FASMassMonsterCombatFragment> Combats = Context.GetMutableFragmentView<FASMassMonsterCombatFragment>();
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
				continue;
			}

			if (Combat.AttackState != EASMassMonsterAttackState::Windup || CurrentTime < Combat.PhaseEndTimeSeconds)
			{
				continue;
			}

			const FVector ExplosionOrigin = Transforms[EntityIt].GetTransform().GetLocation();
			for (const FASMassPlayerTargetSnapshot& Snapshot : TargetSnapshots)
			{
				if (!Snapshot.bAlive || !Snapshot.Pawn.IsValid()
					|| FVector::DistSquared2D(ExplosionOrigin, Snapshot.Location) > FMath::Square(Config.SuicideRadius))
				{
					continue;
				}

				ASMassMonsterDamage::ApplyToActor(Snapshot.Pawn.Get(), Config.AttackPower, ExplosionOrigin);
			}

			Combat.bPrimaryEffectApplied = true;
			Combat.NextAttackTimeSeconds = TNumericLimits<double>::Max();
			Combat.AttackState = EASMassMonsterAttackState::Recovering;
			Health.CurrentHealth = 0.0f;
		}
	});
}
