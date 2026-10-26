#include "Mass/Monster/Processors/ASMassMonsterSeparationProcessor.h"

#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "Movement/MassMovementProcessors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterSeparationProcessor)

namespace ASMassMonsterSeparation
{
	struct FAgent
	{
		FMassEntityHandle Entity;
		FVector Location = FVector::ZeroVector;
		float Radius = 0.0f;
	};

	static FVector MakeFallbackNormal(const FMassEntityHandle A, const FMassEntityHandle B)
	{
		const uint32 Hash = HashCombineFast(GetTypeHash(A), GetTypeHash(B));
		const float Angle = static_cast<float>(Hash % 3600u) * (UE_TWO_PI / 3600.0f);
		return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	}
}

UASMassMonsterSeparationProcessor::UASMassMonsterSeparationProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UMassApplyMovementProcessor::StaticClass()->GetFName());
}

void UASMassMonsterSeparationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHitReactionFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
}

void UASMassMonsterSeparationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	using namespace ASMassMonsterSeparation;

	TArray<FAgent> Agents;
	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	EntityQuery.ForEachEntityChunk(Context, [&Agents, CurrentTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> Radii = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterCombatFragment> Combats = Context.GetFragmentView<FASMassMonsterCombatFragment>();
		const TConstArrayView<FASMassMonsterHitReactionFragment> HitReactions = Context.GetFragmentView<FASMassMonsterHitReactionFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		Agents.Reserve(Agents.Num() + Context.GetNumEntities());
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

			FAgent& Agent = Agents.AddDefaulted_GetRef();
			Agent.Entity = Context.GetEntity(EntityIt);
			Agent.Location = Transforms[EntityIt].GetTransform().GetLocation();
			Agent.Radius = FMath::Max(1.0f, Radii[EntityIt].Radius);
		}
	});

	if (Agents.Num() < 2)
	{
		return;
	}

	float MaxRadius = 1.0f;
	for (const FAgent& Agent : Agents)
	{
		MaxRadius = FMath::Max(MaxRadius, Agent.Radius);
	}
	const float CellSize = FMath::Max(1.0f, MaxRadius * 2.0f + SeparationPadding);
	const int32 IterationCount = FMath::Clamp(SolverIterations, 1, 8);
	const float MaxCorrection = FMath::Max(1.0f, MaxCorrectionPerIteration);

	TMap<FIntPoint, TArray<int32>> Grid;
	TArray<FVector> Corrections;
	Corrections.SetNumZeroed(Agents.Num());

	for (int32 Iteration = 0; Iteration < IterationCount; ++Iteration)
	{
		Grid.Reset();
		for (int32 AgentIndex = 0; AgentIndex < Agents.Num(); ++AgentIndex)
		{
			const FVector& Location = Agents[AgentIndex].Location;
			const FIntPoint Cell(FMath::FloorToInt(Location.X / CellSize), FMath::FloorToInt(Location.Y / CellSize));
			Grid.FindOrAdd(Cell).Add(AgentIndex);
		}

		for (FVector& Correction : Corrections)
		{
			Correction = FVector::ZeroVector;
		}

		for (int32 AgentIndex = 0; AgentIndex < Agents.Num(); ++AgentIndex)
		{
			const FAgent& Agent = Agents[AgentIndex];
			const FIntPoint CenterCell(
				FMath::FloorToInt(Agent.Location.X / CellSize),
				FMath::FloorToInt(Agent.Location.Y / CellSize));

			for (int32 CellY = CenterCell.Y - 1; CellY <= CenterCell.Y + 1; ++CellY)
			{
				for (int32 CellX = CenterCell.X - 1; CellX <= CenterCell.X + 1; ++CellX)
				{
					const TArray<int32>* NeighborIndices = Grid.Find(FIntPoint(CellX, CellY));
					if (!NeighborIndices)
					{
						continue;
					}

					for (const int32 OtherIndex : *NeighborIndices)
					{
						if (OtherIndex <= AgentIndex)
						{
							continue;
						}

						const FAgent& Other = Agents[OtherIndex];
						FVector Delta = Agent.Location - Other.Location;
						Delta.Z = 0.0f;
						const float MinimumDistance = Agent.Radius + Other.Radius + SeparationPadding;
						const float DistanceSquared = Delta.SizeSquared2D();
						if (DistanceSquared >= FMath::Square(MinimumDistance))
						{
							continue;
						}

						const float Distance = FMath::Sqrt(DistanceSquared);
						const FVector Normal = Distance > KINDA_SMALL_NUMBER
							? Delta / Distance
							: MakeFallbackNormal(Agent.Entity, Other.Entity);
						const FVector PairCorrection = Normal * ((MinimumDistance - Distance) * 0.5f);
						Corrections[AgentIndex] += PairCorrection;
						Corrections[OtherIndex] -= PairCorrection;
					}
				}
			}
		}

		for (int32 AgentIndex = 0; AgentIndex < Agents.Num(); ++AgentIndex)
		{
			FVector Correction = Corrections[AgentIndex];
			Correction.Z = 0.0f;
			Agents[AgentIndex].Location += Correction.GetClampedToMaxSize(MaxCorrection);
		}
	}

	TMap<FMassEntityHandle, FVector> ResolvedLocations;
	ResolvedLocations.Reserve(Agents.Num());
	for (const FAgent& Agent : Agents)
	{
		ResolvedLocations.Add(Agent.Entity, Agent.Location);
	}

	EntityQuery.ForEachEntityChunk(Context, [&ResolvedLocations](FMassExecutionContext& Context)
	{
		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			if (const FVector* ResolvedLocation = ResolvedLocations.Find(Context.GetEntity(EntityIt)))
			{
				Transforms[EntityIt].GetMutableTransform().SetLocation(*ResolvedLocation);
			}
		}
	});
}
