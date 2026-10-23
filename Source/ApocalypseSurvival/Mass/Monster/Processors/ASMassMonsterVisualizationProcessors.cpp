#include "Mass/Monster/Processors/ASMassMonsterVisualizationProcessors.h"

#include "Mass/Monster/Processors/ASMassMonsterClientInterpolationProcessor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisionDefinition.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"
#include "MassCommonFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationSubsystem.h"
#include "Mass/Monster/Processors/ASMassMonsterTemporaryVisualProcessor.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterVisualizationProcessors)

namespace
{
	bool HasLocalPlayer(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (const APlayerController* Controller = It->Get(); Controller && Controller->IsLocalController())
			{
				return true;
			}
		}
		return false;
	}
}

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
	: FogVisibilityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	bRequiresGameThreadExecution = true;
}

UASMassMonsterUpdateISMProcessor::UASMassMonsterUpdateISMProcessor()
{
	// 엔진 Processor는 DefaultMass.ini에서 자동 등록을 끈다. 부모 CDO의 Config 값을 물려받지 않도록 명시한다.
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteAfter.Add(UASMassMonsterTemporaryVisualProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UASMassMonsterVisualizationProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UASMassMonsterClientInterpolationProcessor::StaticClass()->GetFName());
}

void UASMassMonsterUpdateISMProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);
	EntityQuery.AddRequirement<FASMassMonsterFogVisibilityFragment>(
		EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UASMassMonsterUpdateISMProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterUpdateISMProcessor);
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& QueryContext)
	{
		UMassRepresentationSubsystem* RepresentationSubsystem =
			QueryContext.GetSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(RepresentationSubsystem);
		FMassInstancedStaticMeshInfoArrayView ISMInfos = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos();

		const TConstArrayView<FTransformFragment> Transforms = QueryContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassRepresentationFragment> Representations =
			QueryContext.GetMutableFragmentView<FMassRepresentationFragment>();
		const TConstArrayView<FMassRepresentationLODFragment> LODs =
			QueryContext.GetFragmentView<FMassRepresentationLODFragment>();
		// 일반 몬스터가 아닌 Entity의 Chunk에서는 비어 있으며, 그때는 엔진과 동일하게 Transform만 쓴다.
		const TConstArrayView<FASMassMonsterFogVisibilityFragment> FogVisibility =
			QueryContext.GetFragmentView<FASMassMonsterFogVisibilityFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = QueryContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassRepresentationFragment& Representation = Representations[EntityIt];
			if (Representation.CurrentRepresentation != EMassRepresentationType::StaticMeshInstance)
			{
				continue;
			}

			const FTransform& Transform = Transforms[EntityIt].GetTransform();
			const FMassRepresentationLODFragment& LOD = LODs[EntityIt];
			const int32 ISMInfoIndex = Representation.StaticMeshDescHandle.ToIndex();
			if (ensureMsgf(ISMInfos.IsValidIndex(ISMInfoIndex), TEXT("Invalid handle index %d for ISMInfosView"), ISMInfoIndex))
			{
				FMassInstancedStaticMeshInfo& ISMInfo = ISMInfos[ISMInfoIndex];
				UpdateISMTransform(QueryContext.GetEntity(EntityIt), ISMInfo, Transform,
					Representation.PrevTransform, LOD.LODSignificance, Representation.PrevLODSignificance);
				if (!FogVisibility.IsEmpty())
				{
					// Transform Batch와 같은 LOD 범위·순서로 한 개의 float를 추가해 Instance와 1:1로 맞춘다.
					const float Visibility = FogVisibility[EntityIt].RenderVisibilityAlpha;
					ISMInfo.AddBatchedCustomData(Visibility, LOD.LODSignificance, Representation.PrevLODSignificance);
				}
			}

			Representation.PrevTransform = Transform;
			Representation.PrevLODSignificance = LOD.LODSignificance;
		}
	});
}

void UASMassMonsterVisualizationProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);
	EntityQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);

	FogVisibilityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	FogVisibilityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadWrite);
	FogVisibilityQuery.AddRequirement<FASMassMonsterFogVisibilityFragment>(EMassFragmentAccess::ReadWrite);
	FogVisibilityQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	FogVisibilityQuery.AddTagRequirement<FMassVisualizationProcessorTag>(EMassFragmentPresence::All);
}

void UASMassMonsterVisualizationProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterVisualizationProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterVisualizationProcessor);

	UWorld* World = GetWorld();
	const UASVisionWorldSubsystem* VisionSubsystem = UASVisionWorldSubsystem::Get(World);
	const AASFogOfWarManager* FogManager = VisionSubsystem ? VisionSubsystem->GetFogOfWarManager() : nullptr;
	const bool bUseFogCulling = World && World->GetNetMode() != NM_DedicatedServer
		&& HasLocalPlayer(World) && FogManager && FogManager->HasLocalVisionMask();

	if (!bUseFogCulling)
	{
		// 안개가 없는 맵에서는 항상 표시한다. 안개는 있지만 첫 시야 마스크가 아직 없으면 위치 노출을 막기 위해 숨긴다.
		const bool bVisibleWithoutMask = FogManager == nullptr;
		FogVisibilityQuery.ForEachEntityChunk(Context, [bVisibleWithoutMask](FMassExecutionContext& QueryContext)
		{
			for (FASMassMonsterFogVisibilityFragment& FogState :
				QueryContext.GetMutableFragmentView<FASMassMonsterFogVisibilityFragment>())
			{
				FogState.bInitialized = true;
				FogState.bVisibleToLocalTeam = bVisibleWithoutMask;
				FogState.bRetainedInFogBuffer = bVisibleWithoutMask;
				FogState.RenderVisibilityAlpha = bVisibleWithoutMask ? 1.0f : 0.0f;
				FogState.SecondsOutsideFogBuffer = 0.0f;
			}
		});
		Super::Execute(EntityManager, Context);
		return;
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	const float CullDelaySeconds = FogManager->GetVisionDefinition()
		? FogManager->GetVisionDefinition()->Settings.MassVisibilityCullDelaySeconds
		: 0.2f;
	const float FadeInSeconds = FogManager->GetVisionDefinition()
		? FogManager->GetVisionDefinition()->Settings.MassVisibilityFadeInSeconds
		: 0.12f;
	FogVisibilityQuery.ForEachEntityChunk(Context, [FogManager, DeltaTime, CullDelaySeconds, FadeInSeconds](FMassExecutionContext& QueryContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = QueryContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassRepresentationLODFragment> LODs =
			QueryContext.GetMutableFragmentView<FMassRepresentationLODFragment>();
		const TArrayView<FASMassMonsterFogVisibilityFragment> FogVisibility =
			QueryContext.GetMutableFragmentView<FASMassMonsterFogVisibilityFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = QueryContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterFogVisibilityFragment& FogState = FogVisibility[EntityIt];
			const FVector Location = Transforms[EntityIt].GetTransform().GetLocation();
			const bool bInLocalTeamVision = FogManager->IsLocationVisibleLocally(Location);
			const bool bInExitRegion = bInLocalTeamVision
				&& FogManager->IsLocationInLocalVisionExitRegion(Location);
			const bool bInReentryRegion = bInLocalTeamVision
				&& FogManager->IsLocationInLocalVisionReentryRegion(Location);
			const bool bInFogBuffer = FogManager->IsLocationInLocalVisionBuffer(Location);

			if (bInLocalTeamVision)
			{
				FogState.bInitialized = true;
				// 보이던 적은 안쪽 숨김선을 벗어나면 즉시 숨기고, 숨겨진 적은 더 안쪽 재진입선까지 들어와야 표시한다.
				const bool bShouldBeVisible = FogState.bVisibleToLocalTeam ? bInExitRegion : bInReentryRegion;
				if (bShouldBeVisible)
				{
					FogState.bVisibleToLocalTeam = true;
					FogState.RenderVisibilityAlpha = FadeInSeconds <= 0.0f
						? 1.0f
						: FMath::Min(1.0f, FogState.RenderVisibilityAlpha + DeltaTime / FadeInSeconds);
				}
				else
				{
					FogState.bVisibleToLocalTeam = false;
					FogState.RenderVisibilityAlpha = 0.0f;
				}
				FogState.bRetainedInFogBuffer = true;
				FogState.SecondsOutsideFogBuffer = 0.0f;
			}
			else if (bInFogBuffer)
			{
				// Buffer 안에서는 ISM을 유지하지만 적 위치가 노출되지 않도록 즉시 숨긴다.
				FogState.bInitialized = true;
				FogState.bVisibleToLocalTeam = false;
				FogState.RenderVisibilityAlpha = 0.0f;
				FogState.bRetainedInFogBuffer = true;
				FogState.SecondsOutsideFogBuffer = 0.0f;
			}
			else
			{
				// Buffer 밖에서는 숨긴 상태를 유지한다. 한 번도 Buffer에 들어오지 않은 Spawn은
				// 바로 Off로 두고, 이미 등록된 ISM만 제거 지연을 적용한다.
				FogState.bVisibleToLocalTeam = false;
				FogState.RenderVisibilityAlpha = 0.0f;
				if (!FogState.bInitialized || !FogState.bRetainedInFogBuffer)
				{
					FogState.bInitialized = true;
					FogState.bRetainedInFogBuffer = false;
					FogState.SecondsOutsideFogBuffer = CullDelaySeconds;
				}
				else
				{
					FogState.SecondsOutsideFogBuffer += DeltaTime;
					if (FogState.SecondsOutsideFogBuffer >= CullDelaySeconds)
					{
						FogState.bRetainedInFogBuffer = false;
					}
				}
			}

			FMassRepresentationLODFragment& LOD = LODs[EntityIt];
			FogState.bTemporarilyForcedOff = !FogState.bRetainedInFogBuffer && LOD.LOD != EMassLOD::Off;
			if (FogState.bTemporarilyForcedOff)
			{
				// 기본 Representation 경로가 ISM instance 제거까지 처리하도록 Off LOD만 한 프레임 강제한다.
				FogState.SavedLOD = LOD.LOD;
				LOD.LOD = EMassLOD::Off;
			}
		}
	});

	Super::Execute(EntityManager, Context);

	// TemporaryVisual/UpdateISM이 정상 거리 LOD를 계속 사용하도록 Super 이후에만 원래 LOD를 되돌린다.
	FogVisibilityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& QueryContext)
	{
		const TArrayView<FMassRepresentationLODFragment> LODs =
			QueryContext.GetMutableFragmentView<FMassRepresentationLODFragment>();
		const TArrayView<FASMassMonsterFogVisibilityFragment> FogVisibility =
			QueryContext.GetMutableFragmentView<FASMassMonsterFogVisibilityFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = QueryContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterFogVisibilityFragment& FogState = FogVisibility[EntityIt];
			if (FogState.bTemporarilyForcedOff)
			{
				LODs[EntityIt].LOD = FogState.SavedLOD;
				FogState.bTemporarilyForcedOff = false;
			}
		}
	});
}
