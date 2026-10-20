#include "Mass/Monster/Processors/ASMassMonsterChargeAttackProcessor.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogASMassMonsterCharge, Log, All);

namespace ASMassMonsterCharge
{
	static bool IsPointWithinSegmentRadius(const FVector& Point, const FVector& Start, const FVector& End, const float Radius)
	{
		const FVector Point2D(Point.X, Point.Y, 0.0f);
		const FVector Start2D(Start.X, Start.Y, 0.0f);
		const FVector End2D(End.X, End.Y, 0.0f);
		return FMath::PointDistToSegmentSquared(Point2D, Start2D, End2D) <= FMath::Square(Radius);
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
	int32 ChargeEntityCount = 0;
	int32 TargetCount = 0;
	int32 InRangeCount = 0;
	int32 ChaseCount = 0;
	int32 BehaviorAttackCount = 0;
	int32 ReadyCount = 0;
	int32 WindupCount = 0;
	int32 ExecutingCount = 0;
	int32 RecoveringCount = 0;
	float MinimumTargetDistance = TNumericLimits<float>::Max();
	float DiagnosticAttackRange = 0.0f;
	float DiagnosticChargeDistance = 0.0f;
	float DiagnosticChargeDuration = 0.0f;
	EntityQuery.ForEachEntityChunk(Context,
		[World, TargetSubsystem, CurrentTime, DeltaTime, &ChargeEntityCount, &TargetCount, &InRangeCount,
			&ChaseCount, &BehaviorAttackCount, &ReadyCount, &WindupCount, &ExecutingCount, &RecoveringCount,
			&MinimumTargetDistance, &DiagnosticAttackRange, &DiagnosticChargeDistance, &DiagnosticChargeDuration]
		(FMassExecutionContext& Context)
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
		DiagnosticAttackRange = Config.AttackRange;
		DiagnosticChargeDistance = Config.ChargeDistance;
		DiagnosticChargeDuration = Config.ChargeDuration;

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			++ChargeEntityCount;
			FASMassMonsterCombatFragment& Combat = Combats[EntityIt];
			FTransform& Transform = Transforms[EntityIt].GetMutableTransform();
			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
			if (Behaviors[EntityIt].State == EASMassMonsterState::Chase)
			{
				++ChaseCount;
			}
			else if (Behaviors[EntityIt].State == EASMassMonsterState::Attack)
			{
				++BehaviorAttackCount;
			}
			if (Targets[EntityIt].HasTarget())
			{
				++TargetCount;
				const float TargetDistance = FVector::Dist2D(
					Transform.GetLocation(), Targets[EntityIt].CachedTargetLocation);
				MinimumTargetDistance = FMath::Min(MinimumTargetDistance, TargetDistance);
				if (TargetDistance <= Config.AttackRange)
				{
					++InRangeCount;
				}
			}

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
				Transform.SetRotation(Direction.Rotation().Quaternion());
				UE_LOG(LogASMassMonsterCharge, Warning,
					TEXT("[MassChargeDiag] Entity=%d Ready->Windup Distance=%.1f Range=%.1f Direction=%s"),
					Entity.Index,
					FVector::Dist2D(Transform.GetLocation(), Targets[EntityIt].CachedTargetLocation),
					Config.AttackRange,
					*Direction.ToCompactString());
				break;
			}
			case EASMassMonsterAttackState::Windup:
				if (CurrentTime >= Combat.PhaseEndTimeSeconds)
				{
					Combat.PhaseEndTimeSeconds = CurrentTime + Config.ChargeDuration;
					Combat.AttackState = EASMassMonsterAttackState::Executing;
					UE_LOG(LogASMassMonsterCharge, Warning,
						TEXT("[MassChargeDiag] Entity=%d Windup->Executing Location=%s"),
						Entity.Index, *Transform.GetLocation().ToCompactString());
				}
				break;

			case EASMassMonsterAttackState::Executing:
			{
				const float SafeDuration = FMath::Max(0.01f, Config.ChargeDuration);
				const float RemainingDistance = FMath::Max(0.0f, Config.ChargeDistance - Combat.ChargeDistanceTraveled);
				const float StepDistance = FMath::Min(Config.ChargeDistance / SafeDuration * DeltaTime, RemainingDistance);
				const FVector Start = Transform.GetLocation();
				const FVector DesiredEnd = Start + Combat.LockedDirection * StepDistance;
				FHitResult BlockingHit;
				const bool bHitWorld = World->SweepSingleByChannel(BlockingHit, Start, DesiredEnd, FQuat::Identity,
					ECC_WorldStatic, FCollisionShape::MakeSphere(Config.ChargeHitRadius));
				const FVector End = bHitWorld ? BlockingHit.Location : DesiredEnd;
				Transform.SetLocation(End);
				Combat.ChargeDistanceTraveled += FVector::Dist2D(Start, End);

				if (!Combat.bPrimaryEffectApplied)
				{
					APawn* TargetPawn = TargetSubsystem->ResolveTargetPawn(Combat.LockedTargetId);
					if (IsValid(TargetPawn)
						&& ASMassMonsterCharge::IsPointWithinSegmentRadius(TargetPawn->GetActorLocation(), Start, End,
							Config.ChargeHitRadius))
					{
						ASMassMonsterDamage::ApplyToActor(TargetPawn, Config.AttackPower, Start);
						Combat.bPrimaryEffectApplied = true;
					}
				}

				if (bHitWorld || Combat.ChargeDistanceTraveled >= Config.ChargeDistance
					|| CurrentTime >= Combat.PhaseEndTimeSeconds)
				{
					UE_LOG(LogASMassMonsterCharge, Warning,
						TEXT("[MassChargeDiag] Entity=%d Executing->Recovering Traveled=%.1f HitWorld=%d TimedOut=%d Location=%s"),
						Entity.Index, Combat.ChargeDistanceTraveled, bHitWorld ? 1 : 0,
						CurrentTime >= Combat.PhaseEndTimeSeconds ? 1 : 0,
						*Transform.GetLocation().ToCompactString());
					Combat.NextAttackTimeSeconds = CurrentTime + Config.AttackCooldown;
					Combat.AttackState = EASMassMonsterAttackState::Recovering;
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

			switch (Combat.AttackState)
			{
			case EASMassMonsterAttackState::Ready: ++ReadyCount; break;
			case EASMassMonsterAttackState::Windup: ++WindupCount; break;
			case EASMassMonsterAttackState::Executing: ++ExecutingCount; break;
			case EASMassMonsterAttackState::Recovering: ++RecoveringCount; break;
			default: break;
			}
		}
	});

	if (CurrentTime >= NextDiagnosticLogTimeSeconds)
	{
		NextDiagnosticLogTimeSeconds = CurrentTime + 1.0;
		const float ReportedMinimumDistance = MinimumTargetDistance == TNumericLimits<float>::Max()
			? -1.0f
			: MinimumTargetDistance;
		UE_LOG(LogASMassMonsterCharge, Warning,
			TEXT("[MassChargeDiag] Tick Entities=%d Targets=%d InRange=%d Behavior[Chase=%d Attack=%d] Combat[Ready=%d Windup=%d Executing=%d Recovering=%d] MinDistance=%.1f AttackRange=%.1f ChargeDistance=%.1f ChargeDuration=%.2f"),
			ChargeEntityCount, TargetCount, InRangeCount, ChaseCount, BehaviorAttackCount,
			ReadyCount, WindupCount, ExecutingCount, RecoveringCount,
			ReportedMinimumDistance, DiagnosticAttackRange, DiagnosticChargeDistance, DiagnosticChargeDuration);
	}
}
