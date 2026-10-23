#include "Mass/Monster/Processors/ASMassMonsterSummonProcessor.h"

#include "Engine/World.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Processors/ASMassMonsterMeleeAttackProcessor.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "NavigationSystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterSummonProcessor)

namespace ASMassMonsterSummon
{
	static TArray<FTransform> FindSpawnTransforms(const UWorld& World, const FVector& Origin,
		const int32 Count, const float MinRadius, const float MaxRadius, const uint32 Seed)
	{
		TArray<FTransform> Transforms;
		const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
		if (!NavigationSystem || Count <= 0)
		{
			return Transforms;
		}

		const float SafeMinRadius = FMath::Max(0.0f, MinRadius);
		const float SafeMaxRadius = FMath::Max(SafeMinRadius, MaxRadius);
		constexpr int32 AttemptsPerMonster = 8;
		for (int32 SpawnIndex = 0; SpawnIndex < Count; ++SpawnIndex)
		{
			for (int32 Attempt = 0; Attempt < AttemptsPerMonster; ++Attempt)
			{
				const uint32 Hash = HashCombineFast(Seed, GetTypeHash(SpawnIndex * AttemptsPerMonster + Attempt));
				const float Angle = static_cast<float>(Hash % 3600u) * (UE_TWO_PI / 3600.0f);
				const float RadiusAlpha = static_cast<float>((Hash / 3600u) % 1000u) / 999.0f;
				const float Radius = FMath::Lerp(SafeMinRadius, SafeMaxRadius, RadiusAlpha);
				const FVector Candidate = Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;
				FNavLocation NavLocation;
				if (NavigationSystem->ProjectPointToNavigation(Candidate, NavLocation, FVector(100.0f, 100.0f, 500.0f)))
				{
					Transforms.Emplace(FRotator::ZeroRotator, NavLocation.Location);
					break;
				}
			}
		}

		return Transforms;
	}
}

UASMassMonsterSummonProcessor::UASMassMonsterSummonProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterMeleeAttackProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterSummonProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);
}

void UASMassMonsterSummonProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterSummonProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterSummonProcessor);

	UWorld* World = GetWorld();
	UASMassMonsterSubsystem* MonsterSubsystem = World ? World->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	if (!World || !MonsterSubsystem)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context, [World, MonsterSubsystem, CurrentTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterTargetFragment> Targets = Context.GetFragmentView<FASMassMonsterTargetFragment>();
		const TArrayView<FASMassMonsterCombatFragment> Combats = Context.GetMutableFragmentView<FASMassMonsterCombatFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		if (Config.AttackPattern != EASMassMonsterAttackPattern::Summon)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterCombatFragment& Combat = Combats[EntityIt];
			if (Combat.bUseFallbackMelee)
			{
				continue;
			}

			if (Combat.AttackState == EASMassMonsterAttackState::Recovering)
			{
				if (CurrentTime >= Combat.NextAttackTimeSeconds)
				{
					Combat.AttackState = EASMassMonsterAttackState::Ready;
				}
				continue;
			}

			if (Combat.AttackState == EASMassMonsterAttackState::Ready)
			{
				if (Combat.PatternActivationCount >= Config.MaxSummonActivations)
				{
					Combat.bUseFallbackMelee = true;
					continue;
				}
				if (CurrentTime < Combat.NextAttackTimeSeconds
					|| Behaviors[EntityIt].State != EASMassMonsterState::Attack
					|| !Targets[EntityIt].HasTarget() || !Config.SummonedEntityConfig)
				{
					continue;
				}

				Combat.LockedTarget = Targets[EntityIt].PrimaryTarget;
				Combat.LockedTargetLocation = Targets[EntityIt].CachedPrimaryTargetLocation;
				Combat.PhaseEndTimeSeconds = CurrentTime + Config.SummonWindupDuration;
				Combat.AttackState = EASMassMonsterAttackState::Windup;
				continue;
			}

			if (Combat.AttackState != EASMassMonsterAttackState::Windup || CurrentTime < Combat.PhaseEndTimeSeconds)
			{
				continue;
			}

			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
			const TArray<FTransform> SpawnTransforms = ASMassMonsterSummon::FindSpawnTransforms(*World,
				Transforms[EntityIt].GetTransform().GetLocation(), Config.SummonCount, Config.SummonMinRadius,
				Config.SummonMaxRadius, GetTypeHash(Entity.Index));
			const bool bQueued = !SpawnTransforms.IsEmpty() && MonsterSubsystem->QueueMonsterSpawns(
				Config.SummonedEntityConfig, SpawnTransforms, Behaviors[EntityIt].WorldBehavior,
				Combat.LockedTarget, Combat.LockedTargetLocation, true);

			if (bQueued)
			{
				++Combat.PatternActivationCount;
			}

			Combat.NextAttackTimeSeconds = CurrentTime + (bQueued ? Config.AttackCooldown : 1.0f);
			Combat.AttackState = EASMassMonsterAttackState::Recovering;
		}
	});
}
