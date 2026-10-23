#include "Mass/Monster/Replication/ASMassMonsterClientBubble.h"

#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Replication/ASMassMonsterClientInterpolationFragment.h"
#include "Mass/Monster/TemporaryVisual/ASMassMonsterTemporaryVisualFragment.h"
#include "MassEntityQuery.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterClientBubble)

namespace
{
	constexpr float ClientHitFlashDuration = 0.12f;
	constexpr float PositionSnapshotTolerance = 1.0f;
	constexpr float YawSnapshotToleranceRadians = 0.00436332313f;
	constexpr float MinimumInterpolationDuration = 0.05f;
	constexpr float MaximumInterpolationDuration = 0.5f;
	constexpr float DefaultInterpolationDuration = 0.1f;
	constexpr float TeleportSnapDistance = 1000.0f;

	float DequantizeHealthRatio(const uint16 HealthRatio)
	{
		return static_cast<float>(HealthRatio) / static_cast<float>(MAX_uint16);
	}

	float GetTransformYawRadians(const FTransform& Transform)
	{
		return FMath::DegreesToRadians(Transform.Rotator().Yaw);
	}
}

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FASMassMonsterClientBubbleHandler::PostReplicatedAdd(
	const TArrayView<int32> AddedIndices, const int32 /*FinalSize*/)
{
	auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& EntityQuery)
	{
		TransformHandler.AddRequirementsForSpawnQuery(EntityQuery);
		EntityQuery.AddRequirement<FASMassMonsterClientInterpolationFragment>(EMassFragmentAccess::ReadWrite);
	};

	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& ExecutionContext)
	{
		TransformHandler.CacheFragmentViewsForSpawnQuery(ExecutionContext);
	};

	auto SetSpawnedEntityData = [this](const FMassEntityView& EntityView,
		const FASMassMonsterReplicatedAgent& Agent, const int32 EntityIndex)
	{
		TransformHandler.SetSpawnedEntityData(EntityIndex, Agent.GetReplicatedPositionYawData());
		InitializeInterpolation(EntityView, Agent);
		ApplyVisualData(EntityView, Agent.GetVisualData(), true);
	};

	auto SetModifiedEntityData = [this](const FMassEntityView& EntityView,
		const FASMassMonsterReplicatedAgent& Agent)
	{
		QueueInterpolationSnapshot(EntityView, Agent);
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
		QueueInterpolationSnapshot(EntityView, Agent);
		ApplyVisualData(EntityView, Agent.GetVisualData(), false);
	};

	PostReplicatedChangeHelper(ChangedIndices, SetModifiedEntityData);
}

void FASMassMonsterClientBubbleHandler::InitializeInterpolation(const FMassEntityView& EntityView,
	const FASMassMonsterReplicatedAgent& Agent) const
{
	const FReplicatedAgentPositionYawData& PositionYaw = Agent.GetReplicatedPositionYawData();
	FASMassMonsterClientInterpolationFragment& Interpolation =
		EntityView.GetFragmentData<FASMassMonsterClientInterpolationFragment>();
	Interpolation.StartLocation = PositionYaw.GetPosition();
	Interpolation.TargetLocation = PositionYaw.GetPosition();
	Interpolation.StartYawRadians = PositionYaw.GetYaw();
	Interpolation.TargetYawRadians = PositionYaw.GetYaw();
	Interpolation.ElapsedSeconds = 0.0f;
	Interpolation.DurationSeconds = 0.0f;
	Interpolation.LastServerTimeSeconds = Agent.GetServerTimeSeconds();
	Interpolation.bInitialized = true;
}

void FASMassMonsterClientBubbleHandler::QueueInterpolationSnapshot(const FMassEntityView& EntityView,
	const FASMassMonsterReplicatedAgent& Agent) const
{
	FASMassMonsterClientInterpolationFragment* Interpolation =
		EntityView.GetFragmentDataPtr<FASMassMonsterClientInterpolationFragment>();
	FTransformFragment* TransformFragment = EntityView.GetFragmentDataPtr<FTransformFragment>();
	if (!Interpolation || !TransformFragment)
	{
		return;
	}

	const FReplicatedAgentPositionYawData& PositionYaw = Agent.GetReplicatedPositionYawData();
	const bool bPositionChanged = !PositionYaw.GetPosition().Equals(
		Interpolation->TargetLocation, PositionSnapshotTolerance);
	const bool bYawChanged = FMath::Abs(FMath::FindDeltaAngleRadians(
		Interpolation->TargetYawRadians, PositionYaw.GetYaw())) > YawSnapshotToleranceRadians;
	if (!bPositionChanged && !bYawChanged)
	{
		return;
	}

	FTransform& CurrentTransform = TransformFragment->GetMutableTransform();
	const float CorrectionDistance = FVector::Dist(CurrentTransform.GetLocation(), PositionYaw.GetPosition());
	if (!Interpolation->bInitialized || CorrectionDistance >= TeleportSnapDistance)
	{
		CurrentTransform.SetLocation(PositionYaw.GetPosition());
		CurrentTransform.SetRotation(FQuat(FVector::UpVector, PositionYaw.GetYaw()));
		InitializeInterpolation(EntityView, Agent);
		return;
	}

	const float ServerInterval = Agent.GetServerTimeSeconds() - Interpolation->LastServerTimeSeconds;
	Interpolation->StartLocation = CurrentTransform.GetLocation();
	Interpolation->TargetLocation = PositionYaw.GetPosition();
	Interpolation->StartYawRadians = GetTransformYawRadians(CurrentTransform);
	Interpolation->TargetYawRadians = PositionYaw.GetYaw();
	Interpolation->ElapsedSeconds = 0.0f;
	Interpolation->DurationSeconds = ServerInterval > UE_SMALL_NUMBER
		? FMath::Clamp(ServerInterval, MinimumInterpolationDuration, MaximumInterpolationDuration)
		: DefaultInterpolationDuration;
	Interpolation->LastServerTimeSeconds = Agent.GetServerTimeSeconds();
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
