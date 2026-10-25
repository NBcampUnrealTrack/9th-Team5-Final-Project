#include "Mass/Monster/Subsystems/ASMassMonsterTargetQuerySubsystem.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntityQuery.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassReplicationFragments.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Replication/ASMassMonsterClientInterpolationFragment.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterTargetQuerySubsystem)

bool UASMassMonsterTargetQuerySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UASMassMonsterTargetQuerySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassEntitySubsystem>();
	EntitySubsystem = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
}

bool UASMassMonsterTargetQuerySubsystem::FindBestAliveMonsterTarget(
	const FVector& SearchOrigin,
	const float SearchRadius,
	const FVector& PreferredLocation,
	const bool bRequireVisibleToLocalTeam,
	uint64& OutTargetId,
	FVector& OutLocation,
	float& OutHitRadius) const
{
	OutTargetId = 0;
	OutLocation = FVector::ZeroVector;
	OutHitRadius = 0.0f;

	const UWorld* World = GetWorld();
	if (!World || SearchRadius < 0.0f)
	{
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		return FindBestClientReplicaTarget(SearchOrigin, SearchRadius, PreferredLocation,
			bRequireVisibleToLocalTeam, OutTargetId, OutLocation, OutHitRadius);
	}

	const UASMassMonsterSubsystem* ServerMonsters = World->GetSubsystem<UASMassMonsterSubsystem>();
	return ServerMonsters && ServerMonsters->FindBestAliveMonsterTarget(SearchOrigin, SearchRadius,
		PreferredLocation, OutTargetId, OutLocation, OutHitRadius);
}

bool UASMassMonsterTargetQuerySubsystem::GetAliveMonsterTarget(
	const uint64 TargetId,
	const bool bRequireVisibleToLocalTeam,
	FVector& OutLocation,
	float& OutHitRadius) const
{
	OutLocation = FVector::ZeroVector;
	OutHitRadius = 0.0f;

	const UWorld* World = GetWorld();
	if (!World || TargetId == 0)
	{
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		return GetAliveClientReplicaTarget(TargetId, bRequireVisibleToLocalTeam,
			OutLocation, OutHitRadius);
	}

	const UASMassMonsterSubsystem* ServerMonsters = World->GetSubsystem<UASMassMonsterSubsystem>();
	return ServerMonsters && ServerMonsters->GetAliveMonsterTarget(TargetId, OutLocation, OutHitRadius);
}

bool UASMassMonsterTargetQuerySubsystem::GetMassNetworkId(
	const uint64 TargetId,
	uint32& OutMassNetworkId) const
{
	OutMassNetworkId = 0;
	if (!EntitySubsystem || TargetId == 0)
	{
		return false;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (EntityManager.IsProcessing())
	{
		return false;
	}

	const FMassEntityHandle Entity = FMassEntityHandle::FromNumber(TargetId);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return false;
	}

	const FMassNetworkIDFragment* NetworkId =
		EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(Entity);
	if (!NetworkId || !NetworkId->NetID.IsValid())
	{
		return false;
	}

	OutMassNetworkId = NetworkId->NetID.GetValue();
	return true;
}

bool UASMassMonsterTargetQuerySubsystem::GetAliveMonsterTargetByNetworkId(
	const uint32 MassNetworkId,
	uint64& OutTargetId,
	FVector& OutLocation,
	float& OutHitRadius) const
{
	OutTargetId = 0;
	OutLocation = FVector::ZeroVector;
	OutHitRadius = 0.0f;

	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || MassNetworkId == 0)
	{
		return false;
	}

	const UASMassMonsterSubsystem* ServerMonsters = World->GetSubsystem<UASMassMonsterSubsystem>();
	return ServerMonsters && ServerMonsters->GetAliveMonsterTargetByNetworkId(
		MassNetworkId, OutTargetId, OutLocation, OutHitRadius);
}

bool UASMassMonsterTargetQuerySubsystem::FindBestClientReplicaTarget(
	const FVector& SearchOrigin,
	const float SearchRadius,
	const FVector& PreferredLocation,
	const bool bRequireVisibleToLocalTeam,
	uint64& OutTargetId,
	FVector& OutLocation,
	float& OutHitRadius) const
{
	OutTargetId = 0;
	OutLocation = FVector::ZeroVector;
	OutHitRadius = 0.0f;
	if (!EntitySubsystem)
	{
		return false;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (EntityManager.IsProcessing())
	{
		return false;
	}

	float BestPreferredDistanceSquared = TNumericLimits<float>::Max();
	float BestOriginDistanceSquared = TNumericLimits<float>::Max();
	FMassEntityQuery Query(EntityManager.AsShared());
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FASMassMonsterFogVisibilityFragment>(EMassFragmentAccess::ReadOnly,
		EMassFragmentPresence::Optional);
	Query.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	Query.AddTagRequirement<FASMassMonsterClientReplicaTag>(EMassFragmentPresence::All);

	FMassExecutionContext Context(EntityManager, 0.0f, false);
	Query.ForEachEntityChunk(Context, [SearchOrigin, SearchRadius, PreferredLocation,
		bRequireVisibleToLocalTeam, &OutTargetId, &OutLocation, &OutHitRadius,
		&BestPreferredDistanceSquared, &BestOriginDistanceSquared](FMassExecutionContext& QueryContext)
	{
		const TConstArrayView<FTransformFragment> Transforms =
			QueryContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FASMassMonsterHealthFragment> Healths =
			QueryContext.GetFragmentView<FASMassMonsterHealthFragment>();
		const TConstArrayView<FASMassMonsterFogVisibilityFragment> FogStates =
			QueryContext.GetFragmentView<FASMassMonsterFogVisibilityFragment>();
		const FASMassMonsterConfigFragment& Config =
			QueryContext.GetConstSharedFragment<FASMassMonsterConfigFragment>();
		const float HitRadius = FMath::Max(0.0f, Config.HitRadius);

		for (FMassExecutionContext::FEntityIterator EntityIt = QueryContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			if (Healths[EntityIt].CurrentHealth <= 0.0f)
			{
				continue;
			}

			if (bRequireVisibleToLocalTeam
				&& (FogStates.IsEmpty() || !FogStates[EntityIt].bInitialized
					|| !FogStates[EntityIt].bVisibleToLocalTeam))
			{
				continue;
			}

			const FVector Location = Transforms[EntityIt].GetTransform().GetLocation();
			const float OriginDistanceSquared = FVector::DistSquared2D(SearchOrigin, Location);
			if (OriginDistanceSquared > FMath::Square(SearchRadius + HitRadius))
			{
				continue;
			}

			const float PreferredDistanceSquared = FVector::DistSquared2D(PreferredLocation, Location);
			if (PreferredDistanceSquared > BestPreferredDistanceSquared
				|| (FMath::IsNearlyEqual(PreferredDistanceSquared, BestPreferredDistanceSquared)
					&& OriginDistanceSquared >= BestOriginDistanceSquared))
			{
				continue;
			}

			const FMassEntityHandle Entity = QueryContext.GetEntity(EntityIt);
			OutTargetId = Entity.AsNumber();
			OutLocation = Location;
			OutHitRadius = HitRadius;
			BestPreferredDistanceSquared = PreferredDistanceSquared;
			BestOriginDistanceSquared = OriginDistanceSquared;
		}
	});

	return OutTargetId != 0;
}

bool UASMassMonsterTargetQuerySubsystem::GetAliveClientReplicaTarget(
	const uint64 TargetId,
	const bool bRequireVisibleToLocalTeam,
	FVector& OutLocation,
	float& OutHitRadius) const
{
	OutLocation = FVector::ZeroVector;
	OutHitRadius = 0.0f;
	if (!EntitySubsystem)
	{
		return false;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (EntityManager.IsProcessing())
	{
		return false;
	}

	const FMassEntityHandle Entity = FMassEntityHandle::FromNumber(TargetId);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return false;
	}

	const FASMassMonsterHealthFragment* Health =
		EntityManager.GetFragmentDataPtr<FASMassMonsterHealthFragment>(Entity);
	const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
	const FASMassMonsterConfigFragment* Config =
		EntityManager.GetConstSharedFragmentDataPtr<FASMassMonsterConfigFragment>(Entity);
	const FASMassMonsterFogVisibilityFragment* FogState =
		EntityManager.GetFragmentDataPtr<FASMassMonsterFogVisibilityFragment>(Entity);
	if (!Health || Health->CurrentHealth <= 0.0f || !Transform || !Config)
	{
		return false;
	}

	if (bRequireVisibleToLocalTeam
		&& (!FogState || !FogState->bInitialized || !FogState->bVisibleToLocalTeam))
	{
		return false;
	}

	OutLocation = Transform->GetTransform().GetLocation();
	OutHitRadius = FMath::Max(0.0f, Config->HitRadius);
	return true;
}
