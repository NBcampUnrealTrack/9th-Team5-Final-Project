#include "Mass/Monster/Replication/ASMassMonsterReplicator.h"

#include "ApocalypseSurvival.h"
#include "Engine/World.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Replication/ASMassMonsterClientBubble.h"
#include "Mass/Monster/Replication/ASMassMonsterReplicatedAgent.h"
#include "MassExecutionContext.h"
#include "MassReplicationTransformHandlers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterReplicator)

namespace
{
	uint16 QuantizeHealthRatio(const float CurrentHealth, const float MaxHealth)
	{
		const float Ratio = MaxHealth > 0.0f
			? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f)
			: 0.0f;
		return static_cast<uint16>(FMath::RoundToInt(Ratio * static_cast<float>(MAX_uint16)));
	}
}

void UASMassMonsterReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	UE_LOG(LogApocalypseSurvival, Log,
		TEXT("[MassMonsterReplication] Replicator AddRequirements: Query=%p Replicator=%s"),
		&EntityQuery, *GetName());

	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);
	EntityQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterRuntimeStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
}

void UASMassMonsterReplicator::ProcessClientReplication(FMassExecutionContext& Context,
	FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	if (!bLoggedProcessEntry)
	{
		bLoggedProcessEntry = true;
		UE_LOG(LogApocalypseSurvival, Log,
			TEXT("[MassMonsterReplication] Replicator ProcessClientReplication entered: Replicator=%s World=%s"),
			*GetName(), *GetNameSafe(&ReplicationContext.World));
	}

	FMassReplicationProcessorPositionYawHandler PositionYawHandler;
	FMassReplicationSharedFragment* ReplicationSharedFragment = nullptr;
	const FMassReplicationParameters* ReplicationParameters = nullptr;
	TConstArrayView<FASMassMonsterHealthFragment> Healths;
	TConstArrayView<FASMassMonsterRuntimeStatsFragment> RuntimeStats;
	TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors;
	TArrayView<FMassReplicatedAgentFragment> ReplicatedAgents;
	const FASMassMonsterConfigFragment* MonsterConfig = nullptr;

	auto BuildVisualData = [&Healths, &RuntimeStats, &Behaviors, &MonsterConfig](const int32 EntityIndex)
	{
		FASReplicatedMonsterVisualData VisualData;
		const FASMassMonsterRuntimeStatsFragment& RuntimeStat = RuntimeStats[EntityIndex];
		const float MaxHealth = RuntimeStat.bInitialized
			? RuntimeStat.MaxHealth
			: MonsterConfig->MaxHealth;
		VisualData.HealthRatio = QuantizeHealthRatio(Healths[EntityIndex].CurrentHealth, MaxHealth);
		VisualData.WorldBehavior = Behaviors[EntityIndex].WorldBehavior;
		return VisualData;
	};

	auto CacheViewsCallback = [&PositionYawHandler, &ReplicationSharedFragment,
		&ReplicationParameters, &Healths, &RuntimeStats, &Behaviors, &ReplicatedAgents, &MonsterConfig](
		FMassExecutionContext& ExecutionContext)
	{
		PositionYawHandler.CacheFragmentViews(ExecutionContext);
		ReplicationSharedFragment = &ExecutionContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		ReplicationParameters = &ExecutionContext.GetConstSharedFragment<FMassReplicationParameters>();
		Healths = ExecutionContext.GetFragmentView<FASMassMonsterHealthFragment>();
		RuntimeStats = ExecutionContext.GetFragmentView<FASMassMonsterRuntimeStatsFragment>();
		Behaviors = ExecutionContext.GetFragmentView<FASMassMonsterBehaviorFragment>();
		ReplicatedAgents = ExecutionContext.GetMutableFragmentView<FMassReplicatedAgentFragment>();
		MonsterConfig = &ExecutionContext.GetConstSharedFragment<FASMassMonsterConfigFragment>();
		check(ReplicationSharedFragment);
		check(ReplicationParameters);
		check(MonsterConfig);
	};

	auto AddEntityCallback = [&ReplicationSharedFragment, &PositionYawHandler, &BuildVisualData](
		FMassExecutionContext& ExecutionContext, const int32 EntityIndex,
		FASMassMonsterReplicatedAgent& ReplicatedAgent, const FMassClientHandle ClientHandle)
		-> FMassReplicatedAgentHandle
	{
		AASMassMonsterClientBubbleInfo& BubbleInfo =
			ReplicationSharedFragment->GetTypedClientBubbleInfoChecked<AASMassMonsterClientBubbleInfo>(ClientHandle);
		FASMassMonsterClientBubbleSerializer& Serializer = BubbleInfo.GetMonsterSerializer();

		PositionYawHandler.AddEntity(EntityIndex, ReplicatedAgent.GetReplicatedPositionYawDataMutable());
		ReplicatedAgent.GetServerTimeSecondsMutable() = static_cast<float>(ExecutionContext.GetWorld()->GetTimeSeconds());
		ReplicatedAgent.GetVisualDataMutable() = BuildVisualData(EntityIndex);
		return Serializer.GetBubble().AddAgent(ExecutionContext.GetEntity(EntityIndex), ReplicatedAgent);
	};

	auto ModifyEntityCallback = [&ReplicationSharedFragment, &ReplicationParameters, &PositionYawHandler,
		&ReplicatedAgents, &BuildVisualData](FMassExecutionContext& ExecutionContext, const int32 EntityIndex,
		const EMassLOD::Type LOD, const double Time, const FMassReplicatedAgentHandle Handle,
		const FMassClientHandle ClientHandle)
	{
		FMassReplicatedAgentData& AgentData = ReplicatedAgents[EntityIndex].AgentData;
		const double UpdateInterval = ReplicationParameters->UpdateInterval[LOD];
		if (Time - AgentData.LastUpdateTime < UpdateInterval)
		{
			return;
		}

		AASMassMonsterClientBubbleInfo& BubbleInfo =
			ReplicationSharedFragment->GetTypedClientBubbleInfoChecked<AASMassMonsterClientBubbleInfo>(ClientHandle);
		FASMassMonsterClientBubbleHandler& Bubble = BubbleInfo.GetMonsterSerializer().GetBubble();
		const FReplicatedAgentPositionYawData PreviousPositionYaw =
			Bubble.GetAgentChecked(Handle).GetReplicatedPositionYawData();
		PositionYawHandler.ModifyEntity(Handle, EntityIndex, Bubble.GetTransformHandlerMutable());
		const FReplicatedAgentPositionYawData& CurrentPositionYaw =
			Bubble.GetAgentChecked(Handle).GetReplicatedPositionYawData();
		if (!CurrentPositionYaw.GetPosition().Equals(PreviousPositionYaw.GetPosition(),
			UE::Mass::Replication::PositionReplicateTolerance)
			|| FMath::Abs(FMath::FindDeltaAngleRadians(CurrentPositionYaw.GetYaw(),
				PreviousPositionYaw.GetYaw())) > UE::Mass::Replication::YawReplicateTolerance)
		{
			Bubble.GetAgentItem(Handle).Agent.GetServerTimeSecondsMutable() = static_cast<float>(Time);
		}
		Bubble.SetVisualData(Handle, BuildVisualData(EntityIndex));
		AgentData.LastUpdateTime = Time;
	};

	auto RemoveEntityCallback = [&ReplicationSharedFragment](FMassExecutionContext& ExecutionContext,
		const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AASMassMonsterClientBubbleInfo& BubbleInfo =
			ReplicationSharedFragment->GetTypedClientBubbleInfoChecked<AASMassMonsterClientBubbleInfo>(ClientHandle);
		BubbleInfo.GetMonsterSerializer().GetBubble().RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FASMassMonsterFastArrayItem>(Context, ReplicationContext,
		CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif
}
