#include "Mass/Monster/Processors/ASMassMonsterDeathProcessor.h"

#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Processors/ASMassMonsterBehaviorProcessor.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterDeathProcessor)

UASMassMonsterDeathProcessor::UASMassMonsterDeathProcessor()
	: NewlyDeadQuery(*this)
	, DeadQuery(*this)
	, PendingDestroyQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterBehaviorProcessor::StaticClass()->GetFName());
}

void UASMassMonsterDeathProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NewlyDeadQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadWrite);
	NewlyDeadQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadWrite);
	NewlyDeadQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadWrite);
	NewlyDeadQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	NewlyDeadQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);

	DeadQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadWrite);
	DeadQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::All);
	DeadQuery.AddTagRequirement<FASMassMonsterPendingDestroyTag>(EMassFragmentPresence::None);

	PendingDestroyQuery.AddTagRequirement<FASMassMonsterPendingDestroyTag>(EMassFragmentPresence::All);
}

void UASMassMonsterDeathProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterDeathProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterDeathProcessor);

	NewlyDeadQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const TArrayView<FASMassMonsterHealthFragment> Healths = Context.GetMutableFragmentView<FASMassMonsterHealthFragment>();
		const TArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetMutableFragmentView<FASMassMonsterBehaviorFragment>();
		const TArrayView<FASMassMonsterTargetFragment> Targets = Context.GetMutableFragmentView<FASMassMonsterTargetFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterHealthFragment& Health = Healths[EntityIt];
			if (Health.CurrentHealth > 0.0f)
			{
				continue;
			}

			Health.CurrentHealth = 0.0f;
			Health.DeathTimeRemaining = Config.DeathDespawnDelay;
			Health.bDeathTimerStarted = true;
			Behaviors[EntityIt].State = EASMassMonsterState::Dead;
			Targets[EntityIt].Reset();
			Context.Defer().AddTag<FASMassMonsterDeadTag>(Context.GetEntity(EntityIt));
		}
	});

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	DeadQuery.ForEachEntityChunk(Context, [DeltaTime](FMassExecutionContext& Context)
	{
		const TArrayView<FASMassMonsterHealthFragment> Healths = Context.GetMutableFragmentView<FASMassMonsterHealthFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterHealthFragment& Health = Healths[EntityIt];
			if (!Health.bDeathTimerStarted)
			{
				continue;
			}

			Health.DeathTimeRemaining = FMath::Max(0.0f, Health.DeathTimeRemaining - DeltaTime);
			if (Health.DeathTimeRemaining <= 0.0f)
			{
				Context.Defer().AddTag<FASMassMonsterPendingDestroyTag>(Context.GetEntity(EntityIt));
			}
		}
	});

	PendingDestroyQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			Context.Defer().DestroyEntity(Context.GetEntity(EntityIt));
		}
	});
}
