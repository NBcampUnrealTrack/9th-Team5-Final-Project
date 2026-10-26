#include "Mass/Monster/Processors/ASMassMonsterHasteSupportProcessor.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Engine/World.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Processors/ASMassMonsterBehaviorProcessor.h"
#include "Mass/Monster/Processors/ASMassMonsterNavigationProcessor.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterHasteSupportProcessor)

namespace ASMassMonsterHaste
{
	struct FHastePulse
	{
		FMassEntityHandle SourceEntity;
		FVector Origin = FVector::ZeroVector;
		float Radius = 0.0f;
		float Multiplier = 1.0f;
		double EndTimeSeconds = 0.0;
	};

	static void SpawnIndicator(UWorld& World, const FASMassMonsterConfigFragment& Config, const FVector& Origin)
	{
		if (!Config.HasteIndicatorClass || Config.HasteWindupDuration <= 0.0f)
		{
			return;
		}

		const FTransform SpawnTransform(FRotator::ZeroRotator, Origin);
		AASMonsterAttackIndicatorActor* Indicator = World.SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
			Config.HasteIndicatorClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Indicator)
		{
			return;
		}

		Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Circle, Config.HasteRadius,
			0.0f, 0.0f, Config.HasteWindupDuration);
		Indicator->FinishSpawning(SpawnTransform);
		Indicator->SetLifeSpan(Config.HasteWindupDuration);
	}
}

UASMassMonsterHasteSupportProcessor::UASMassMonsterHasteSupportProcessor()
	: HasteSourceQuery(*this)
	, ModifierQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteAfter.Add(UASMassMonsterBehaviorProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterNavigationProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterHasteSupportProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	HasteSourceQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	HasteSourceQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	HasteSourceQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadOnly);
	HasteSourceQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadWrite);
	HasteSourceQuery.AddRequirement<FASMassMonsterHitReactionFragment>(EMassFragmentAccess::ReadOnly);
	HasteSourceQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	HasteSourceQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);

	ModifierQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	ModifierQuery.AddRequirement<FASMassMonsterModifierFragment>(EMassFragmentAccess::ReadWrite);
	ModifierQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	ModifierQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);
}

void UASMassMonsterHasteSupportProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterHasteSupportProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterHasteSupportProcessor);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	TArray<ASMassMonsterHaste::FHastePulse> Pulses;
	HasteSourceQuery.ForEachEntityChunk(Context, [World, CurrentTime, &Pulses](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterTargetFragment> Targets = Context.GetFragmentView<FASMassMonsterTargetFragment>();
		const TArrayView<FASMassMonsterCombatFragment> Combats = Context.GetMutableFragmentView<FASMassMonsterCombatFragment>();
		const TConstArrayView<FASMassMonsterHitReactionFragment> HitReactions = Context.GetFragmentView<FASMassMonsterHitReactionFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		if (Config.AttackPattern != EASMassMonsterAttackPattern::HasteSupport)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterCombatFragment& Combat = Combats[EntityIt];
			if (HitReactions[EntityIt].IsControlActive(CurrentTime))
			{
				Combat.AttackState = EASMassMonsterAttackState::Ready;
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
				if (CurrentTime < Combat.NextAttackTimeSeconds
					|| Behaviors[EntityIt].State != EASMassMonsterState::Attack
					|| !Targets[EntityIt].HasTarget())
				{
					continue;
				}

				Combat.PhaseEndTimeSeconds = CurrentTime + Config.HasteWindupDuration;
				Combat.AttackState = EASMassMonsterAttackState::Windup;
				ASMassMonsterHaste::SpawnIndicator(*World, Config,
					Transforms[EntityIt].GetTransform().GetLocation());
				continue;
			}

			if (Combat.AttackState != EASMassMonsterAttackState::Windup || CurrentTime < Combat.PhaseEndTimeSeconds)
			{
				continue;
			}

			ASMassMonsterHaste::FHastePulse& Pulse = Pulses.AddDefaulted_GetRef();
			Pulse.SourceEntity = Context.GetEntity(EntityIt);
			Pulse.Origin = Transforms[EntityIt].GetTransform().GetLocation();
			Pulse.Radius = Config.HasteRadius;
			Pulse.Multiplier = Config.HasteMoveSpeedMultiplier;
			Pulse.EndTimeSeconds = CurrentTime + Config.HasteDuration;
			Combat.NextAttackTimeSeconds = CurrentTime + Config.AttackCooldown;
			Combat.AttackState = EASMassMonsterAttackState::Recovering;
		}
	});

	ModifierQuery.ForEachEntityChunk(Context, [CurrentTime, &Pulses](FMassExecutionContext& Context)
	{
		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FASMassMonsterModifierFragment> Modifiers = Context.GetMutableFragmentView<FASMassMonsterModifierFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterModifierFragment& Modifier = Modifiers[EntityIt];
			FTransform& Transform = Transforms[EntityIt].GetMutableTransform();
			if (!Modifier.bBaseVisualScaleInitialized)
			{
				Modifier.BaseVisualScale = Transform.GetScale3D();
				Modifier.bBaseVisualScaleInitialized = true;
			}
			if (CurrentTime >= Modifier.HasteEndTimeSeconds)
			{
				Modifier.MoveSpeedMultiplier = 1.0f;
				Modifier.HasteEndTimeSeconds = 0.0;
			}

			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
			const FVector EntityLocation = Transforms[EntityIt].GetTransform().GetLocation();
			for (const ASMassMonsterHaste::FHastePulse& Pulse : Pulses)
			{
				if (Entity == Pulse.SourceEntity
					|| FVector::DistSquared2D(EntityLocation, Pulse.Origin) > FMath::Square(Pulse.Radius))
				{
					continue;
				}

				Modifier.MoveSpeedMultiplier = FMath::Max(Modifier.MoveSpeedMultiplier, Pulse.Multiplier);
				Modifier.HasteEndTimeSeconds = FMath::Max(Modifier.HasteEndTimeSeconds, Pulse.EndTimeSeconds);
			}

			// 버프 중인 일반 몬스터는 인스턴스 크기를 키워 별도 BP/VFX 없이도 상태를 식별할 수 있게 한다.
			const bool bHasHaste = CurrentTime < Modifier.HasteEndTimeSeconds;
			Transform.SetScale3D(Modifier.BaseVisualScale * (bHasHaste ? 1.15f : 1.0f));
		}
	});
}
