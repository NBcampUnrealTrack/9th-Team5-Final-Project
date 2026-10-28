#include "Mass/Monster/Processors/ASMassMonsterActorSeparationProcessor.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Processors/ASMassMonsterSeparationProcessor.h"
#include "Mass/Monster/Subsystems/ASMonsterTargetSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterActorSeparationProcessor)

namespace ASMassMonsterActorSeparation
{
	struct FObstacle
	{
		FVector Location = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector2D BoxHalfExtent = FVector2D::ZeroVector;
		float Radius = 0.0f;
		uint32 StableHash = 0;
		bool bIsBox = false;
	};

	static FVector MakeFallbackNormal(const FMassEntityHandle Entity, const uint32 ObstacleHash)
	{
		const uint32 Hash = HashCombineFast(GetTypeHash(Entity), ObstacleHash);
		const float Angle = static_cast<float>(Hash % 3600u) * (UE_TWO_PI / 3600.0f);
		return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	}

	static float MakeFallbackSign(const FMassEntityHandle Entity, const uint32 ObstacleHash, const uint32 AxisSalt)
	{
		return (HashCombineFast(HashCombineFast(GetTypeHash(Entity), ObstacleHash), AxisSalt) & 1u) != 0u
			? 1.0f
			: -1.0f;
	}

	static FVector CalculateCorrection(const FObstacle& Obstacle, const FMassEntityHandle Entity,
		const FVector& AgentLocation, const float AgentRadius, const float Padding)
	{
		const float Clearance = AgentRadius + Padding;
		if (!Obstacle.bIsBox)
		{
			FVector Delta = AgentLocation - Obstacle.Location;
			Delta.Z = 0.0f;
			const float MinimumDistance = Clearance + Obstacle.Radius;
			const float DistanceSquared = Delta.SizeSquared2D();
			if (DistanceSquared >= FMath::Square(MinimumDistance))
			{
				return FVector::ZeroVector;
			}

			const float Distance = FMath::Sqrt(DistanceSquared);
			const FVector Normal = Distance > KINDA_SMALL_NUMBER
				? Delta / Distance
				: MakeFallbackNormal(Entity, Obstacle.StableHash);
			return Normal * (MinimumDistance - Distance);
		}

		FVector LocalLocation = Obstacle.Rotation.UnrotateVector(AgentLocation - Obstacle.Location);
		LocalLocation.Z = 0.0f;
		const FVector2D ClosestPoint(
			FMath::Clamp(LocalLocation.X, -Obstacle.BoxHalfExtent.X, Obstacle.BoxHalfExtent.X),
			FMath::Clamp(LocalLocation.Y, -Obstacle.BoxHalfExtent.Y, Obstacle.BoxHalfExtent.Y));
		const FVector2D LocalDelta(LocalLocation.X - ClosestPoint.X, LocalLocation.Y - ClosestPoint.Y);
		const float DistanceSquared = LocalDelta.SizeSquared();

		FVector LocalCorrection = FVector::ZeroVector;
		if (DistanceSquared > KINDA_SMALL_NUMBER)
		{
			if (DistanceSquared >= FMath::Square(Clearance))
			{
				return FVector::ZeroVector;
			}

			const float Distance = FMath::Sqrt(DistanceSquared);
			const FVector2D Correction2D = LocalDelta / Distance * (Clearance - Distance);
			LocalCorrection.X = Correction2D.X;
			LocalCorrection.Y = Correction2D.Y;
		}
		else
		{
			// The agent center is inside the box footprint. Push it through the nearest expanded face.
			const float PenetrationX = Obstacle.BoxHalfExtent.X + Clearance - FMath::Abs(LocalLocation.X);
			const float PenetrationY = Obstacle.BoxHalfExtent.Y + Clearance - FMath::Abs(LocalLocation.Y);
			if (PenetrationX <= PenetrationY)
			{
				const float Sign = FMath::Abs(LocalLocation.X) > KINDA_SMALL_NUMBER
					? FMath::Sign(LocalLocation.X)
					: MakeFallbackSign(Entity, Obstacle.StableHash, 0x58u);
				LocalCorrection.X = Sign * PenetrationX;
			}
			else
			{
				const float Sign = FMath::Abs(LocalLocation.Y) > KINDA_SMALL_NUMBER
					? FMath::Sign(LocalLocation.Y)
					: MakeFallbackSign(Entity, Obstacle.StableHash, 0x59u);
				LocalCorrection.Y = Sign * PenetrationY;
			}
		}

		return Obstacle.Rotation.RotateVector(LocalCorrection);
	}
}

UASMassMonsterActorSeparationProcessor::UASMassMonsterActorSeparationProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UASMassMonsterSeparationProcessor::StaticClass()->GetFName());
}

void UASMassMonsterActorSeparationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHitReactionFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
}

void UASMassMonsterActorSeparationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	using namespace ASMassMonsterActorSeparation;

	check(IsInGameThread());
	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FObstacle> Obstacles;
	auto AddCapsuleObstacle = [&Obstacles](const ACharacter* Character)
	{
		const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		if (!IsValid(Character) || !Capsule || Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			return;
		}

		FObstacle& Obstacle = Obstacles.AddDefaulted_GetRef();
		Obstacle.Location = Capsule->GetComponentLocation();
		Obstacle.Radius = FMath::Max(1.0f, Capsule->GetScaledCapsuleRadius());
		Obstacle.StableHash = PointerHash(Character);
	};

	for (TActorIterator<AASMonsterCharacter> It(World); It; ++It)
	{
		const AASMonsterCharacter* Monster = *It;
		if (!IsValid(Monster) || Monster->IsDead())
		{
			continue;
		}
		AddCapsuleObstacle(Monster);
	}

	for (TActorIterator<AASPlayerCharacter> It(World); It; ++It)
	{
		const AASPlayerCharacter* Player = *It;
		if (!IsValid(Player) || Player->IsDead())
		{
			continue;
		}
		AddCapsuleObstacle(Player);
	}

	if (const UASMonsterTargetSubsystem* TargetSubsystem = World->GetSubsystem<UASMonsterTargetSubsystem>())
	{
		TArray<FASTargetSnapshot> BlockingTargets;
		TargetSubsystem->GetTargetsWithCapabilities(
			EASMonsterTargetCapability::BlockingTarget, BlockingTargets);
		for (const FASTargetSnapshot& Snapshot : BlockingTargets)
		{
			const AActor* Actor = Snapshot.Actor.Get();
			const UPrimitiveComponent* RootPrimitive = Actor
				? Cast<UPrimitiveComponent>(Actor->GetRootComponent())
				: nullptr;
			if (!IsValid(Actor) || !RootPrimitive
				|| RootPrimitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}

			FObstacle& Obstacle = Obstacles.AddDefaulted_GetRef();
			Obstacle.Location = RootPrimitive->GetComponentLocation();
			Obstacle.StableHash = GetTypeHash(Snapshot.Handle);
			if (const UBoxComponent* Box = Cast<UBoxComponent>(RootPrimitive))
			{
				const FVector Extent = Box->GetScaledBoxExtent();
				Obstacle.bIsBox = true;
				Obstacle.BoxHalfExtent = FVector2D(
					FMath::Max(1.0f, Extent.X), FMath::Max(1.0f, Extent.Y));
				Obstacle.Rotation = FQuat(
					FVector::UpVector, FMath::DegreesToRadians(Box->GetComponentRotation().Yaw));
			}
			else
			{
				Obstacle.Radius = FMath::Max(1.0f, Snapshot.HitRadius);
			}
		}
	}

	if (Obstacles.IsEmpty())
	{
		return;
	}

	const float Padding = FMath::Max(0.0f, SeparationPadding);
	const float MaxCorrection = FMath::Max(1.0f, MaxCorrectionPerFrame);
	const double CurrentTime = World->GetTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context, [&Obstacles, Padding, MaxCorrection, CurrentTime](FMassExecutionContext& Context)
	{
		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> Radii = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterCombatFragment> Combats = Context.GetFragmentView<FASMassMonsterCombatFragment>();
		const TConstArrayView<FASMassMonsterHitReactionFragment> HitReactions = Context.GetFragmentView<FASMassMonsterHitReactionFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const bool bIgnoresSeparation =
				(Config.AttackPattern == EASMassMonsterAttackPattern::Charge
					&& Combats[EntityIt].AttackState == EASMassMonsterAttackState::Executing)
				|| (Config.AttackPattern == EASMassMonsterAttackPattern::Suicide
					&& Combats[EntityIt].AttackState == EASMassMonsterAttackState::Windup);
			if (Behaviors[EntityIt].State == EASMassMonsterState::Dead
				|| bIgnoresSeparation || HitReactions[EntityIt].IsControlActive(CurrentTime))
			{
				continue;
			}

			FTransform& Transform = Transforms[EntityIt].GetMutableTransform();
			const FVector AgentLocation = Transform.GetLocation();
			const float AgentRadius = FMath::Max(1.0f, Radii[EntityIt].Radius);
			FVector Correction = FVector::ZeroVector;

			for (const FObstacle& Obstacle : Obstacles)
			{
				Correction += CalculateCorrection(
					Obstacle, Context.GetEntity(EntityIt), AgentLocation, AgentRadius, Padding);
			}

			Correction.Z = 0.0f;
			Transform.SetLocation(AgentLocation + Correction.GetClampedToMaxSize(MaxCorrection));
		}
	});
}
