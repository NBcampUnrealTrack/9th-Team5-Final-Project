#include "Mass/Monster/Processors/ASMassMonsterChargeAttackProcessor.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Engine/World.h"
#include "Mass/Monster/Combat/ASMassMonsterDamageBridge.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Processors/ASMassMonsterBehaviorProcessor.h"
#include "Mass/Monster/Processors/ASMassMonsterNavigationProcessor.h"
#include "Mass/Monster/Subsystems/ASMassPlayerTargetSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterChargeAttackProcessor)

namespace ASMassMonsterCharge
{
	static bool IsPointWithinSegmentRadius(const FVector& Point, const FVector& Start, const FVector& End, const float Radius)
	{
		const FVector Point2D(Point.X, Point.Y, 0.0f);
		const FVector Start2D(Start.X, Start.Y, 0.0f);
		const FVector End2D(End.X, End.Y, 0.0f);
		return FMath::PointDistToSegmentSquared(Point2D, Start2D, End2D) <= FMath::Square(Radius);
	}

	static void SpawnIndicator(UWorld& World, const FASMassMonsterConfigFragment& Config,
		const FVector& Origin, const FVector& Direction)
	{
		if (!Config.ChargeIndicatorClass || Config.ChargeWindupDuration <= 0.0f)
		{
			return;
		}

		const FVector IndicatorLocation = Origin + Direction * (Config.ChargeDistance * 0.5f);
		const FTransform SpawnTransform(Direction.Rotation(), IndicatorLocation);
		AASMonsterAttackIndicatorActor* Indicator = World.SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
			Config.ChargeIndicatorClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Indicator)
		{
			return;
		}

		Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Rectangle, 0.0f,
			Config.ChargeDistance, Config.ChargeHitRadius * 2.0f, Config.ChargeWindupDuration);
		Indicator->FinishSpawning(SpawnTransform);
		Indicator->SetLifeSpan(Config.ChargeWindupDuration);
	}
}

UASMassMonsterChargeAttackProcessor::UASMassMonsterChargeAttackProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteAfter.Add(UASMassMonsterBehaviorProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterNavigationProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterChargeAttackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);
}

void UASMassMonsterChargeAttackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterChargeAttackProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterChargeAttackProcessor);

	UWorld* World = GetWorld();
	UASMassPlayerTargetSubsystem* TargetSubsystem = World ? World->GetSubsystem<UASMassPlayerTargetSubsystem>() : nullptr;
	if (!World || !TargetSubsystem)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	const TConstArrayView<FASMassPlayerTargetSnapshot> TargetSnapshots = TargetSubsystem->GetTargetSnapshots();
	EntityQuery.ForEachEntityChunk(Context, [this, World, CurrentTime, DeltaTime, TargetSnapshots](FMassExecutionContext& Context)
	{
		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterTargetFragment> Targets = Context.GetFragmentView<FASMassMonsterTargetFragment>();
		const TArrayView<FASMassMonsterCombatFragment> Combats = Context.GetMutableFragmentView<FASMassMonsterCombatFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		if (Config.AttackPattern != EASMassMonsterAttackPattern::Charge)
		{
			return;
		}
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterCombatFragment& Combat = Combats[EntityIt];
			FTransform& Transform = Transforms[EntityIt].GetMutableTransform();
			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);

			switch (Combat.AttackState)
			{
			case EASMassMonsterAttackState::Ready:
			{
				if (CurrentTime < Combat.NextAttackTimeSeconds)
				{
					Combat.AttackState = EASMassMonsterAttackState::Recovering;
					break;
				}
				if (!Targets[EntityIt].HasTarget()
					|| (Behaviors[EntityIt].State != EASMassMonsterState::Chase
						&& Behaviors[EntityIt].State != EASMassMonsterState::Attack))
				{
					break;
				}
				if (FVector::DistSquared2D(Transform.GetLocation(), Targets[EntityIt].CachedTargetLocation)
					> FMath::Square(Config.AttackRange))
				{
					break;
				}

				const FVector Direction = (Targets[EntityIt].CachedTargetLocation - Transform.GetLocation()).GetSafeNormal2D();
				if (Direction.IsNearlyZero())
				{
					break;
				}

				Combat.LockedDirection = Direction;
				Combat.LockedTargetLocation = Targets[EntityIt].CachedTargetLocation;
				Combat.LockedTargetId = Targets[EntityIt].TargetId;
				Combat.ChargeDistanceTraveled = 0.0f;
				Combat.bPrimaryEffectApplied = false;
				Combat.PhaseEndTimeSeconds = CurrentTime + Config.ChargeWindupDuration;
				Combat.AttackState = EASMassMonsterAttackState::Windup;
				HitTargetIdsByEntity.Remove(Entity);
				Transform.SetRotation(Direction.Rotation().Quaternion());
				ASMassMonsterCharge::SpawnIndicator(*World, Config, Transform.GetLocation(), Direction);
				break;
			}
			case EASMassMonsterAttackState::Windup:
				if (CurrentTime >= Combat.PhaseEndTimeSeconds)
				{
					Combat.PhaseEndTimeSeconds = CurrentTime + Config.ChargeDuration;
					Combat.AttackState = EASMassMonsterAttackState::Executing;
				}
				break;

			case EASMassMonsterAttackState::Executing:
			{
				const float SafeDuration = FMath::Max(0.01f, Config.ChargeDuration);
				const float RemainingDistance = FMath::Max(0.0f, Config.ChargeDistance - Combat.ChargeDistanceTraveled);
				const float StepDistance = FMath::Min(Config.ChargeDistance / SafeDuration * DeltaTime, RemainingDistance);
				const FVector Start = Transform.GetLocation();
				const FVector End = Start + Combat.LockedDirection * StepDistance;
				Transform.SetLocation(End);
				Combat.ChargeDistanceTraveled += FVector::Dist2D(Start, End);

				TSet<int32>& HitTargetIds = HitTargetIdsByEntity.FindOrAdd(Entity);
				for (const FASMassPlayerTargetSnapshot& Snapshot : TargetSnapshots)
				{
					if (!Snapshot.bAlive || !Snapshot.Pawn.IsValid() || HitTargetIds.Contains(Snapshot.TargetId)
						|| !ASMassMonsterCharge::IsPointWithinSegmentRadius(
							Snapshot.Location, Start, End, Config.ChargeHitRadius))
					{
						continue;
					}

					ASMassMonsterDamage::ApplyToActor(Snapshot.Pawn.Get(), Config.AttackPower, Start);
					HitTargetIds.Add(Snapshot.TargetId);
					Combat.bPrimaryEffectApplied = true;
				}

				if (Combat.ChargeDistanceTraveled >= Config.ChargeDistance
					|| CurrentTime >= Combat.PhaseEndTimeSeconds)
				{
					Combat.NextAttackTimeSeconds = CurrentTime + Config.AttackCooldown;
					Combat.AttackState = EASMassMonsterAttackState::Recovering;
					HitTargetIdsByEntity.Remove(Entity);
				}
				break;
			}

			case EASMassMonsterAttackState::Recovering:
				if (CurrentTime >= Combat.NextAttackTimeSeconds)
				{
					Combat.AttackState = EASMassMonsterAttackState::Ready;
				}
				break;

			default:
				break;
			}
		}
	});
}
