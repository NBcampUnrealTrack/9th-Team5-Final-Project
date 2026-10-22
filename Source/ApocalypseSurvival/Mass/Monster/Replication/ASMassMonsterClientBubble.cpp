#include "Mass/Monster/Replication/ASMassMonsterClientBubble.h"

#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/TemporaryVisual/ASMassMonsterTemporaryVisualFragment.h"
#include "MassEntityQuery.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterClientBubble)

namespace
{
	constexpr float ClientHitFlashDuration = 0.12f;

	float DequantizeHealthRatio(const uint16 HealthRatio)
	{
		return static_cast<float>(HealthRatio) / static_cast<float>(MAX_uint16);
	}
}

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FASMassMonsterClientBubbleHandler::PostReplicatedAdd(
	const TArrayView<int32> AddedIndices, const int32 /*FinalSize*/)
{
	auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& EntityQuery)
	{
		TransformHandler.AddRequirementsForSpawnQuery(EntityQuery);
	};

	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& ExecutionContext)
	{
		TransformHandler.CacheFragmentViewsForSpawnQuery(ExecutionContext);
	};

	auto SetSpawnedEntityData = [this](const FMassEntityView& EntityView,
		const FASMassMonsterReplicatedAgent& Agent, const int32 EntityIndex)
	{
		TransformHandler.SetSpawnedEntityData(EntityIndex, Agent.GetReplicatedPositionYawData());
		ApplyVisualData(EntityView, Agent.GetVisualData(), true);
	};

	auto SetModifiedEntityData = [this](const FMassEntityView& EntityView,
		const FASMassMonsterReplicatedAgent& Agent)
	{
		TransformHandler.SetModifiedEntityData(EntityView, Agent.GetReplicatedPositionYawData());
		ApplyVisualData(EntityView, Agent.GetVisualData(), false);
	};

	PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery,
		CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);
	TransformHandler.ClearFragmentViewsForSpawnQuery();
}

void FASMassMonsterClientBubbleHandler::PostReplicatedChange(
	const TArrayView<int32> ChangedIndices, const int32 /*FinalSize*/)
{
	auto SetModifiedEntityData = [this](const FMassEntityView& EntityView,
		const FASMassMonsterReplicatedAgent& Agent)
	{
		TransformHandler.SetModifiedEntityData(EntityView, Agent.GetReplicatedPositionYawData());
		ApplyVisualData(EntityView, Agent.GetVisualData(), false);
	};

	PostReplicatedChangeHelper(ChangedIndices, SetModifiedEntityData);
}

void FASMassMonsterClientBubbleHandler::ApplyVisualData(const FMassEntityView& EntityView,
	const FASReplicatedMonsterVisualData& VisualData, const bool bIsInitialState) const
{
	const float NewHealthRatio = DequantizeHealthRatio(VisualData.HealthRatio);
	bool bReceivedDamage = false;
	if (FASMassMonsterHealthFragment* Health = EntityView.GetFragmentDataPtr<FASMassMonsterHealthFragment>())
	{
		const float PreviousHealthRatio = Health->CurrentHealth;
		Health->CurrentHealth = NewHealthRatio;
		bReceivedDamage = !bIsInitialState
			&& PreviousHealthRatio > 0.0f
			&& NewHealthRatio > 0.0f
			&& NewHealthRatio < PreviousHealthRatio;
	}

	if (FASMassMonsterTemporaryVisualFragment* TemporaryVisual =
		EntityView.GetFragmentDataPtr<FASMassMonsterTemporaryVisualFragment>())
	{
		TemporaryVisual->SpawnWorldBehavior = VisualData.WorldBehavior;
		if (bReceivedDamage)
		{
			TemporaryVisual->HitFlashTimeRemaining = ClientHitFlashDuration;
		}
	}
}
#endif

AASMassMonsterClientBubbleInfo::AASMassMonsterClientBubbleInfo(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Serializers.Add(&MonsterSerializer);
}

void AASMassMonsterClientBubbleInfo::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AASMassMonsterClientBubbleInfo, MonsterSerializer, SharedParams);
}
