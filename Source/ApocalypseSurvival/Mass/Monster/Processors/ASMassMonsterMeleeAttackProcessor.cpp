#include "Mass/Monster/Processors/ASMassMonsterMeleeAttackProcessor.h"

#include "Engine/World.h"
#include "Mass/Monster/Combat/ASMassMonsterDamageBridge.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Subsystems/ASMonsterTargetSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterMeleeAttackProcessor)

UASMassMonsterMeleeAttackProcessor::UASMassMonsterMeleeAttackProcessor()
	: EntityQuery(*this)
{
	// DefaultMass.ini 또는 맵별 Processor 설정에 의존하지 않고 모든 게임 월드에서 근접 공격을 등록한다.
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterMeleeAttackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterRuntimeStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);
}

void UASMassMonsterMeleeAttackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterMeleeAttackProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterMeleeAttackProcessor);

	UWorld* World = GetWorld();
	UASMonsterTargetSubsystem* TargetSubsystem = World ? World->GetSubsystem<UASMonsterTargetSubsystem>() : nullptr;
	if (!World || !TargetSubsystem)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context, [TargetSubsystem, CurrentTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterTargetFragment> Targets = Context.GetFragmentView<FASMassMonsterTargetFragment>();
		const TArrayView<FASMassMonsterCombatFragment> Combats = Context.GetMutableFragmentView<FASMassMonsterCombatFragment>();
		const TConstArrayView<FASMassMonsterRuntimeStatsFragment> RuntimeStats = Context.GetFragmentView<FASMassMonsterRuntimeStatsFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterCombatFragment& Combat = Combats[EntityIt];
			const bool bUsesMelee = Config.AttackPattern == EASMassMonsterAttackPattern::Melee
				|| Combat.bUseFallbackMelee;
			if (!bUsesMelee)
			{
				continue;
			}

			if (CurrentTime < Combat.NextAttackTimeSeconds)
			{
				Combat.AttackState = EASMassMonsterAttackState::Recovering;
				continue;
			}

			Combat.AttackState = EASMassMonsterAttackState::Ready;
			if (Behaviors[EntityIt].State != EASMassMonsterState::Attack
				|| !Targets[EntityIt].HasTarget())
			{
				continue;
			}

			FASTargetSnapshot TargetSnapshot;
			const FVector SourceLocation = Transforms[EntityIt].GetTransform().GetLocation();
			const float EffectiveAttackRange = Combat.bUseFallbackMelee
				? Config.FallbackMeleeAttackRange
				: Config.AttackRange + Config.HitRadius;
			const float EffectiveAttackPower = Combat.bUseFallbackMelee
				? Config.FallbackMeleeAttackPower
				: RuntimeStats[EntityIt].AttackPower;
			const float EffectiveCooldown = Combat.bUseFallbackMelee
				? Config.FallbackMeleeAttackCooldown
				: Config.AttackCooldown;
			if (!TargetSubsystem->ResolveTarget(Targets[EntityIt].PrimaryTarget, TargetSnapshot)
				|| !TargetSnapshot.bAlive || !TargetSnapshot.bAttackable || !TargetSnapshot.Actor.IsValid()
				|| FVector::DistSquared2D(SourceLocation, TargetSnapshot.Location) > FMath::Square(EffectiveAttackRange))
			{
				continue;
			}

			ASMassMonsterDamage::ApplyToActor(TargetSnapshot.Actor.Get(), EffectiveAttackPower, SourceLocation);
			Combat.NextAttackTimeSeconds = CurrentTime + EffectiveCooldown;
			Combat.AttackState = EASMassMonsterAttackState::Recovering;
		}
	});
}
