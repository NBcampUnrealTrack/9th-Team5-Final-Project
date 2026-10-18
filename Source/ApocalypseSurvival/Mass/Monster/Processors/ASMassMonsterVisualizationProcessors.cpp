#include "Mass/Monster/Processors/ASMassMonsterVisualizationProcessors.h"

#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterVisualizationProcessors)

UASMassMonsterLODCollectorProcessor::UASMassMonsterLODCollectorProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UASMassMonsterLODCollectorProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);

	EntityQuery_VisibleRangeAndOnLOD.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	EntityQuery_VisibleRangeOnly.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	EntityQuery_OnLODOnly.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	EntityQuery_NotVisibleRangeAndOffLOD.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
}

void UASMassMonsterLODCollectorProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterLODCollectorProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterLODCollectorProcessor);
	Super::Execute(EntityManager, Context);
}

UASMassMonsterVisualizationLODProcessor::UASMassMonsterVisualizationLODProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UASMassMonsterVisualizationLODProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);

	CloseEntityQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	CloseEntityAdjustDistanceQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	FarEntityQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	DebugEntityQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	FilterTag = FASMassMonsterTag::StaticStruct();
}

void UASMassMonsterVisualizationLODProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterVisualizationLODProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterVisualizationLODProcessor);
	Super::Execute(EntityManager, Context);
}

UASMassMonsterVisualizationProcessor::UASMassMonsterVisualizationProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterVisualizationProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);
	EntityQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
}

void UASMassMonsterVisualizationProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterVisualizationProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterVisualizationProcessor);
	Super::Execute(EntityManager, Context);
}
