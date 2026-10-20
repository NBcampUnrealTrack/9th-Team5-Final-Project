#include "Mass/Monster/Processors/ASMassMonsterActorSeparationProcessor.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Processors/ASMassMonsterSeparationProcessor.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterActorSeparationProcessor)

namespace ASMassMonsterActorSeparation
{
	struct FObstacle
	{
		FVector Location = FVector::ZeroVector;
		float Radius = 0.0f;
		uint32 StableHash = 0;
	};

	static FVector MakeFallbackNormal(const FMassEntityHandle Entity, const uint32 ObstacleHash)
	{
		const uint32 Hash = HashCombineFast(GetTypeHash(Entity), ObstacleHash);
		const float Angle = static_cast<float>(Hash % 3600u) * (UE_TWO_PI / 3600.0f);
		return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
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

	if (Obstacles.IsEmpty())
	{
		return;
	}

	const float Padding = FMath::Max(0.0f, SeparationPadding);
	const float MaxCorrection = FMath::Max(1.0f, MaxCorrectionPerFrame);
	EntityQuery.ForEachEntityChunk(Context, [&Obstacles, Padding, MaxCorrection](FMassExecutionContext& Context)
	{
		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> Radii = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			if (Behaviors[EntityIt].State == EASMassMonsterState::Dead)
			{
				continue;
			}

			FTransform& Transform = Transforms[EntityIt].GetMutableTransform();
			const FVector AgentLocation = Transform.GetLocation();
			const float AgentRadius = FMath::Max(1.0f, Radii[EntityIt].Radius);
			FVector Correction = FVector::ZeroVector;

			for (const FObstacle& Obstacle : Obstacles)
			{
				FVector Delta = AgentLocation - Obstacle.Location;
				Delta.Z = 0.0f;
				const float MinimumDistance = AgentRadius + Obstacle.Radius + Padding;
				const float DistanceSquared = Delta.SizeSquared2D();
				if (DistanceSquared >= FMath::Square(MinimumDistance))
				{
					continue;
				}

				const float Distance = FMath::Sqrt(DistanceSquared);
				const FVector Normal = Distance > KINDA_SMALL_NUMBER
					? Delta / Distance
					: MakeFallbackNormal(Context.GetEntity(EntityIt), Obstacle.StableHash);
				Correction += Normal * (MinimumDistance - Distance);
			}

			Correction.Z = 0.0f;
			Transform.SetLocation(AgentLocation + Correction.GetClampedToMaxSize(MaxCorrection));
		}
	});
}
