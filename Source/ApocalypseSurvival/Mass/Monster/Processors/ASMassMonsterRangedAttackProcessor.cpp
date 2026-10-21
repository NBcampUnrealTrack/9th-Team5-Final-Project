#include "Mass/Monster/Processors/ASMassMonsterRangedAttackProcessor.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Mass/Monster/Combat/ASMassMonsterProjectile.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Subsystems/ASMassPlayerTargetSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterRangedAttackProcessor)

UASMassMonsterRangedAttackProcessor::UASMassMonsterRangedAttackProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterRangedAttackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);
}

void UASMassMonsterRangedAttackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterRangedAttackProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterRangedAttackProcessor);

	UWorld* World = GetWorld();
	UASMassPlayerTargetSubsystem* TargetSubsystem = World
		? World->GetSubsystem<UASMassPlayerTargetSubsystem>()
		: nullptr;
	if (!World || !TargetSubsystem)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context, [World, TargetSubsystem, CurrentTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetFragmentView<FASMassMonsterBehaviorFragment>();
		const TConstArrayView<FASMassMonsterTargetFragment> Targets = Context.GetFragmentView<FASMassMonsterTargetFragment>();
		const TArrayView<FASMassMonsterCombatFragment> Combats = Context.GetMutableFragmentView<FASMassMonsterCombatFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		if (Config.AttackPattern != EASMassMonsterAttackPattern::RangedProjectile)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			if (Config.AttackPattern != EASMassMonsterAttackPattern::RangedProjectile)
			{
				continue;
			}

			FASMassMonsterCombatFragment& Combat = Combats[EntityIt];
			if (CurrentTime < Combat.NextAttackTimeSeconds)
			{
				Combat.AttackState = EASMassMonsterAttackState::Recovering;
				continue;
			}

			Combat.AttackState = EASMassMonsterAttackState::Ready;
			if (Behaviors[EntityIt].State != EASMassMonsterState::Attack || !Targets[EntityIt].HasTarget())
			{
				continue;
			}

			APawn* TargetPawn = TargetSubsystem->ResolveTargetPawn(Targets[EntityIt].TargetId);
			const FVector EntityLocation = Transforms[EntityIt].GetTransform().GetLocation();
			if (!IsValid(TargetPawn)
				|| FVector::DistSquared2D(EntityLocation, TargetPawn->GetActorLocation()) > FMath::Square(Config.AttackRange))
			{
				continue;
			}

			const FVector SpawnLocation = EntityLocation + FVector(0.0f, 0.0f, Config.RangedProjectileSpawnHeight);
			FVector Direction = (TargetPawn->GetActorLocation() - SpawnLocation).GetSafeNormal();
			if (Direction.IsNearlyZero())
			{
				continue;
			}

			const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
			UClass* ProjectileClass = Config.RangedProjectileClass
				? Config.RangedProjectileClass.Get()
				: AASMassMonsterProjectile::StaticClass();
			AASMassMonsterProjectile* Projectile = World->SpawnActorDeferred<AASMassMonsterProjectile>(
				ProjectileClass, SpawnTransform, nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Projectile)
			{
				continue;
			}

			Projectile->InitializeProjectile(Config.AttackPower, EntityLocation, Direction,
				Config.RangedProjectileSpeed, Config.RangedProjectileMaxRange);
			Projectile->FinishSpawning(SpawnTransform);
			Combat.NextAttackTimeSeconds = CurrentTime + Config.AttackCooldown;
			Combat.AttackState = EASMassMonsterAttackState::Recovering;
		}
	});
}
