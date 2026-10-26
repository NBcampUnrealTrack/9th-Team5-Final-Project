#include "Mass/Replication/ASMassReplicationProcessor.h"

#include "ApocalypseSurvival.h"
#include "HAL/IConsoleManager.h"
#include "MassCommonFragments.h"
#include "MassEntityUtils.h"
#include "MassExecutionContext.h"
#include "MassLODSubsystem.h"
#include "Mass/Monster/Replication/ASMassMonsterClientBubble.h"
#include "Mass/Monster/Replication/ASMassMonsterReplicator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassReplicationProcessor)

UASMassReplicationProcessor::UASMassReplicationProcessor()
	: MonsterReplicationQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
}

void UASMassReplicationProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);

	// UMassReplicationProcessor::EntityQuery의 공통 replication 요구사항을 명시적으로 복제한다.
	// 복사 Query를 등록하면 Query 주소가 바뀌므로, 이 Processor 멤버 Query에 직접 선언해야 한다.
	MonsterReplicationQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
	MonsterReplicationQuery.AddRequirement<FReplicationTemplateIDFragment>(EMassFragmentAccess::ReadOnly);
	MonsterReplicationQuery.AddRequirement<FMassReplicationLODFragment>(EMassFragmentAccess::ReadWrite);
	MonsterReplicationQuery.AddRequirement<FMassReplicatedAgentFragment>(EMassFragmentAccess::ReadWrite);
	MonsterReplicationQuery.AddConstSharedRequirement<FMassReplicationParameters>();
	MonsterReplicationQuery.AddSharedRequirement<FMassReplicationSharedFragment>(EMassFragmentAccess::ReadWrite);

	// MonsterReplicationQuery is constructed with (*this), which has already registered it with
	// UMassProcessor. AddRequirements remains declarative; a new monster Replicator type requires
	// a separate fixed member query so the Processing Queue can track its access requirements.
	GetMutableDefault<UASMassMonsterReplicator>()->AddRequirements(MonsterReplicationQuery);
}

void UASMassReplicationProcessor::Execute(
	FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	UWorld* World = EntityManager.GetWorld();
	check(World);
	check(ReplicationSubsystem);

	{
		QUICK_SCOPE_CYCLE_COUNTER(UASMassReplicationProcessor_Preparation);
		PrepareExecution(EntityManager);
	}

	const UMassLODSubsystem& LODSubsystem = Context.GetSubsystemChecked<UMassLODSubsystem>();
	const TArray<FViewerInfo>& AllViewersInfo = LODSubsystem.GetViewers();
	const TArray<FMassClientHandle>& ClientHandles = ReplicationSubsystem->GetClientReplicationHandles();
	for (const FMassClientHandle ClientHandle : ClientHandles)
	{
		if (!ReplicationSubsystem->IsValidClientHandle(ClientHandle))
		{
			continue;
		}

		FMassClientReplicationInfo& ClientReplicationInfo =
			ReplicationSubsystem->GetMutableClientReplicationInfoChecked(ClientHandle);

		TArray<FViewerInfo> Viewers;
		for (const FMassViewerHandle ClientViewerHandle : ClientReplicationInfo.Handles)
		{
			const FViewerInfo* ViewerInfo = AllViewersInfo.FindByPredicate(
				[ClientViewerHandle](const FViewerInfo& Candidate)
				{
					return ClientViewerHandle == Candidate.Handle;
				});
			if (ensureMsgf(ViewerInfo, TEXT("Expecting to find the client viewer handle in the all viewers info list")))
			{
				Viewers.Add(*ViewerInfo);
			}
		}

		float MaxLODDistance = 0.0f;
		EntityManager.ForEachSharedFragment<FMassReplicationSharedFragment>(
			[&Viewers, &MaxLODDistance](FMassReplicationSharedFragment& RepSharedFragment)
			{
				RepSharedFragment.LODCollector.PrepareExecution(Viewers);
				RepSharedFragment.LODCalculator.PrepareExecution(Viewers);
				MaxLODDistance = FMath::Max(
					MaxLODDistance, RepSharedFragment.LODCalculator.GetMaxLODDistance());
			});

		const FVector HalfExtent(MaxLODDistance, MaxLODDistance, 0.0f);
		TArray<FMassEntityHandle> EntitiesInRange;
		for (const FViewerInfo& Viewer : Viewers)
		{
			const FBox Bounds(Viewer.Location - HalfExtent, Viewer.Location + HalfExtent);
			ReplicationSubsystem->GetGrid().Query(Bounds, EntitiesInRange);
		}

		EntitiesInRange.Append(ClientReplicationInfo.HandledEntities);
		TArray<FMassArchetypeEntityCollection> EntitySets;
		UE::Mass::Utils::CreateEntityCollections(
			Context.GetEntityManagerChecked(),
			EntitiesInRange,
			FMassArchetypeEntityCollection::FoldDuplicates,
			EntitySets);

		TArray<FMassEntityHandle> EntitiesHandledThisFrame;

		for (FMassArchetypeEntityCollection& Set : EntitySets)
		{
			checkf(
				!Set.IsEmpty(),
				TEXT("We don't expect to get empty collections from UE::Mass::Utils::CreateEntityCollections"));

			Context.SetEntityCollection(Set);
			Set.ExportEntityHandles(EntitiesHandledThisFrame);

			{
				QUICK_SCOPE_CYCLE_COUNTER(UASMassReplicationProcessor_SyncToMass);
				SyncClientData.ForEachEntityChunk(
					Context,
					[&ClientReplicationInfo](FMassExecutionContext& QueryContext)
					{
						const TArrayView<FMassReplicationLODFragment> ViewerLODList =
							QueryContext.GetMutableFragmentView<FMassReplicationLODFragment>();
						const TArrayView<FMassReplicatedAgentFragment> ReplicatedAgentList =
							QueryContext.GetMutableFragmentView<FMassReplicatedAgentFragment>();

						for (FMassExecutionContext::FEntityIterator EntityIt = QueryContext.CreateEntityIterator();
							EntityIt;
							++EntityIt)
						{
							const FMassEntityHandle EntityHandle = QueryContext.GetEntity(EntityIt);
							FMassReplicatedAgentFragment& AgentFragment = ReplicatedAgentList[EntityIt];
							FMassReplicationLODFragment& LODFragment = ViewerLODList[EntityIt];

							if (FMassReplicatedAgentData* AgentData =
								ClientReplicationInfo.AgentsData.Find(EntityHandle))
							{
								LODFragment.LOD = AgentData->LOD;
								AgentFragment.AgentData = *AgentData;
							}
							else
							{
								LODFragment.LOD = EMassLOD::Off;
								AgentFragment.AgentData.Invalidate();
							}
						}
					});
			}

			{
				QUICK_SCOPE_CYCLE_COUNTER(UASMassReplicationProcessor_LODCollection);
				CollectViewerInfoQuery.ForEachEntityChunk(
					Context,
					[](FMassExecutionContext& QueryContext)
					{
						const TConstArrayView<FTransformFragment> LocationList =
							QueryContext.GetFragmentView<FTransformFragment>();
						const TArrayView<FMassReplicationViewerInfoFragment> ViewersInfoList =
							QueryContext.GetMutableFragmentView<FMassReplicationViewerInfoFragment>();
						FMassReplicationSharedFragment& RepSharedFragment =
							QueryContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
						RepSharedFragment.LODCollector.CollectLODInfo(
							QueryContext, LocationList, ViewersInfoList, ViewersInfoList);
					});
			}

			{
				QUICK_SCOPE_CYCLE_COUNTER(UASMassReplicationProcessor_LODCalculation);
				CalculateLODQuery.ForEachEntityChunk(
					Context,
					[](FMassExecutionContext& QueryContext)
					{
						const TConstArrayView<FMassReplicationViewerInfoFragment> ViewersInfoList =
							QueryContext.GetFragmentView<FMassReplicationViewerInfoFragment>();
						const TArrayView<FMassReplicationLODFragment> ViewerLODList =
							QueryContext.GetMutableFragmentView<FMassReplicationLODFragment>();
						FMassReplicationSharedFragment& RepSharedFragment =
							QueryContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
						RepSharedFragment.LODCalculator.CalculateLOD(
							QueryContext, ViewersInfoList, ViewerLODList, ViewersInfoList);
					});
			}

			Context.ClearEntityCollection();
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(UASMassReplicationProcessor_LODAdjustDistance);
			EntityManager.ForEachSharedFragment<FMassReplicationSharedFragment>(
				[](FMassReplicationSharedFragment& RepSharedFragment)
				{
					RepSharedFragment.bHasAdjustedDistancesFromCount =
						RepSharedFragment.LODCalculator.AdjustDistancesFromCount();
				});
		}

		// The owned monster query reads the Shared Fragment for every processed chunk. Set the
		// active client once before executing it so all monster configurations use this client.
		EntityManager.ForEachSharedFragment<FMassReplicationSharedFragment>(
			[&ClientHandle](FMassReplicationSharedFragment& RepSharedFragment)
			{
				RepSharedFragment.CurrentClientHandle = ClientHandle;
			});

		for (FMassArchetypeEntityCollection& Set : EntitySets)
		{
			Context.SetEntityCollection(Set);

			{
				QUICK_SCOPE_CYCLE_COUNTER(UASMassReplicationProcessor_LODAdjustLODFromCount);
				AdjustLODDistancesQuery.ForEachEntityChunk(
					Context,
					[](FMassExecutionContext& QueryContext)
					{
						const TConstArrayView<FMassReplicationViewerInfoFragment> ViewersInfoList =
							QueryContext.GetFragmentView<FMassReplicationViewerInfoFragment>();
						const TArrayView<FMassReplicationLODFragment> ViewerLODList =
							QueryContext.GetMutableFragmentView<FMassReplicationLODFragment>();
						FMassReplicationSharedFragment& RepSharedFragment =
							QueryContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
						RepSharedFragment.LODCalculator.AdjustLODFromCount(
							QueryContext, ViewersInfoList, ViewerLODList, ViewersInfoList);
					});
			}

			{
				QUICK_SCOPE_CYCLE_COUNTER(UASMassReplicationProcessor_ProcessClientReplication);
				FMassReplicationContext ReplicationContext(*World, LODSubsystem, *ReplicationSubsystem);
				MonsterReplicationQuery.ForEachEntityChunk(
					Context,
					[&ReplicationContext](FMassExecutionContext& QueryContext)
					{
						FMassReplicationSharedFragment& RepSharedFragment =
							QueryContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
						checkf(
							RepSharedFragment.CachedReplicator
								&& RepSharedFragment.CachedReplicator->IsA<UASMassMonsterReplicator>(),
							TEXT("UASMassReplicationProcessor only supports UASMassMonsterReplicator. Add a separate processor-owned query for other replicator types."));
						RepSharedFragment.CachedReplicator->ProcessClientReplication(
							QueryContext, ReplicationContext);
					});
			}

			{
				QUICK_SCOPE_CYCLE_COUNTER(UASMassReplicationProcessor_SyncFromMass);
				SyncClientData.ForEachEntityChunk(
					Context,
					[&ClientReplicationInfo](FMassExecutionContext& QueryContext)
					{
						const TArrayView<FMassReplicatedAgentFragment> ReplicatedAgentList =
							QueryContext.GetMutableFragmentView<FMassReplicatedAgentFragment>();

						for (FMassExecutionContext::FEntityIterator EntityIt = QueryContext.CreateEntityIterator();
							EntityIt;
							++EntityIt)
						{
							const FMassEntityHandle EntityHandle = QueryContext.GetEntity(EntityIt);
							const FMassReplicatedAgentFragment& AgentFragment = ReplicatedAgentList[EntityIt];
							ClientReplicationInfo.AgentsData.Add(EntityHandle, AgentFragment.AgentData);
						}
					});
			}

#if WITH_MASSGAMEPLAY_DEBUG
			const IConsoleVariable* DebugLODVariable =
				IConsoleManager::Get().FindConsoleVariable(TEXT("mass.debug.ClientReplicationLOD"));
			if (DebugLODVariable && DebugLODVariable->GetInt() == ClientHandle.GetIndex())
			{
				MonsterReplicationQuery.ForEachEntityChunk(
					Context,
					[World](FMassExecutionContext& QueryContext)
					{
						const TConstArrayView<FTransformFragment> TransformList =
							QueryContext.GetFragmentView<FTransformFragment>();
						const TConstArrayView<FMassReplicationLODFragment> ViewerLODList =
							QueryContext.GetFragmentView<FMassReplicationLODFragment>();
						FMassReplicationSharedFragment& RepSharedFragment =
							QueryContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
						RepSharedFragment.LODCalculator.DebugDisplayLOD(
							QueryContext, ViewerLODList, TransformList, World);
					});
			}
#endif

			Context.ClearEntityCollection();
		}

		ClientReplicationInfo.HandledEntities = MoveTemp(EntitiesHandledThisFrame);

		// Entity가 이미 파괴돼 이번 프레임의 Collection에서 빠지면 CalculateClientReplication의
		// RemoveEntityCallback까지 도달하지 못한다. 해당 Agent는 여전히 Bubble에 남아 있고,
		// 다음 정리 assert가 발생한다. 이 Processor는 UASMassMonsterReplicator 전용이므로
		// 같은 Client Bubble에서 남은 Handle을 명시적으로 제거할 수 있다.
		const FMassBubbleInfoClassHandle MonsterBubbleClassHandle =
			ReplicationSubsystem->GetBubbleInfoClassHandle(AASMassMonsterClientBubbleInfo::StaticClass());
		AASMassMonsterClientBubbleInfo* MonsterBubbleInfo =
			ReplicationSubsystem->GetTypedClientBubble<AASMassMonsterClientBubbleInfo>(
				MonsterBubbleClassHandle, ClientHandle);

		for (FMassReplicationAgentDataMap::TIterator It =
				ClientReplicationInfo.AgentsData.CreateIterator();
			It;
			++It)
		{
			FMassReplicatedAgentData& AgentData = It.Value();
			if (AgentData.bPendingDestruction || AgentData.LOD == EMassLOD::Off)
			{
				if (AgentData.Handle.IsValid())
				{
					if (ensureMsgf(MonsterBubbleInfo,
						TEXT("[MassMonsterReplication] Missing client bubble while cleaning a stale replicated agent.")))
					{
						const bool bRemoved = MonsterBubbleInfo->GetMonsterSerializer().GetBubble().RemoveAgent(AgentData.Handle);
						UE_LOG(LogApocalypseSurvival, Warning,
							TEXT("[MassMonsterReplication] Fallback cleanup Entity=%d PendingDestroy=%d LOD=%d Removed=%d"),
							It.Key().Index, AgentData.bPendingDestruction, static_cast<int32>(AgentData.LOD), bRemoved);
					}
					AgentData.Invalidate();
				}
				It.RemoveCurrent();
			}
		}
	}
#endif
}
