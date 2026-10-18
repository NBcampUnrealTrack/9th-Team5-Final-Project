#include "Mass/Monster/Processors/ASMassMonsterMeleeAttackProcessor.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Mass/Monster/Combat/ASMassMonsterDamageBridge.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Targeting/ASMassPlayerTargetSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterMeleeAttackProcessor)

UASMassMonsterMeleeAttackProcessor::UASMassMonsterMeleeAttackProcessor()
	: EntityQuery(*this)
{
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
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);
}

void UASMassMonsterMeleeAttackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = GetWorld();
	UASMassPlayerTargetSubsystem* TargetSubsystem = World ? World->GetSubsystem<UASMassPlayerTargetSubsystem>() : nullptr;
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
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterCombatFragment& Combat = Combats[EntityIt];
			if (CurrentTime < Combat.NextAttackTimeSeconds)
			{
				Combat.AttackState = EASMassMonsterAttackState::Recovering;
				continue;
			}

			Combat.AttackState = EASMassMonsterAttackState::Ready;
			if (Config.AttackPattern != EASMassMonsterAttackPattern::Melee
				|| Behaviors[EntityIt].State != EASMassMonsterState::Attack
				|| !Targets[EntityIt].HasTarget())
			{
				continue;
			}

			APawn* TargetPawn = TargetSubsystem->ResolveTargetPawn(Targets[EntityIt].TargetId);
			const FVector SourceLocation = Transforms[EntityIt].GetTransform().GetLocation();
			if (!IsValid(TargetPawn)
				|| FVector::DistSquared2D(SourceLocation, TargetPawn->GetActorLocation()) > FMath::Square(Config.AttackRange))
			{
				continue;
			}

			ASMassMonsterDamage::ApplyToActor(TargetPawn, Config.AttackPower, SourceLocation);
			Combat.NextAttackTimeSeconds = CurrentTime + Config.AttackCooldown;
			Combat.AttackState = EASMassMonsterAttackState::Recovering;
		}
	});
}
