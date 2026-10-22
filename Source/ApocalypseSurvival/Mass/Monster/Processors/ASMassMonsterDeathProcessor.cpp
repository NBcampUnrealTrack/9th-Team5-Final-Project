#include "Mass/Monster/Processors/ASMassMonsterDeathProcessor.h"

#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Processors/ASMassMonsterBehaviorProcessor.h"
#include "Core/Framework/ASGameMode.h"
#include "Core/Framework/ASPlayerState.h"
#include "Characters/Player/Components/ASProgressionComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Items/Loot/ASMonsterDropSubsystem.h"
#include "MassCommonFragments.h"
#include "Mass/Monster/Subsystems/ASMassPlayerTargetSubsystem.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterDeathProcessor)

namespace
{
	struct FASPendingMonsterDrop
	{
		FDataTableRowHandle DropGroup;
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<AController> KillerController;
	};
}

UASMassMonsterDeathProcessor::UASMassMonsterDeathProcessor()
	: NewlyDeadQuery(*this)
	, DeadQuery(*this)
	, PendingDestroyQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterBehaviorProcessor::StaticClass()->GetFName());
	// 사망 집계와 경험치 지급은 GameMode, PlayerState, TimerManager를 만지므로 게임 스레드에서 실행한다.
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterDeathProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NewlyDeadQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadWrite);
	NewlyDeadQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	NewlyDeadQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadWrite);
	NewlyDeadQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadWrite);
	NewlyDeadQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
	NewlyDeadQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::None);

	DeadQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadWrite);
	DeadQuery.AddTagRequirement<FASMassMonsterDeadTag>(EMassFragmentPresence::All);
	DeadQuery.AddTagRequirement<FASMassMonsterPendingDestroyTag>(EMassFragmentPresence::None);

	PendingDestroyQuery.AddTagRequirement<FASMassMonsterPendingDestroyTag>(EMassFragmentPresence::All);
}

void UASMassMonsterDeathProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterDeathProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterDeathProcessor);

	int32 NewlyDeadCount = 0;
	TMap<TWeakObjectPtr<AController>, int32> ExperienceByController;
	TArray<FASPendingMonsterDrop> PendingDrops;
	const UASMassPlayerTargetSubsystem* PlayerTargets = GetWorld()
		? GetWorld()->GetSubsystem<UASMassPlayerTargetSubsystem>()
		: nullptr;
	NewlyDeadQuery.ForEachEntityChunk(Context, [&NewlyDeadCount, &ExperienceByController, &PendingDrops, PlayerTargets](FMassExecutionContext& Context)
	{
		const TArrayView<FASMassMonsterHealthFragment> Healths = Context.GetMutableFragmentView<FASMassMonsterHealthFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetMutableFragmentView<FASMassMonsterBehaviorFragment>();
		const TArrayView<FASMassMonsterTargetFragment> Targets = Context.GetMutableFragmentView<FASMassMonsterTargetFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterHealthFragment& Health = Healths[EntityIt];
			if (Health.CurrentHealth > 0.0f)
			{
				continue;
			}

			Health.CurrentHealth = 0.0f;
			Health.DeathTimeRemaining = Config.DeathDespawnDelay;
			Health.bDeathTimerStarted = true;
			if (PlayerTargets)
			{
				if (const FASMassPlayerTargetSnapshot* KillerSnapshot =
					PlayerTargets->FindTarget(Health.LastDamageInstigatorTargetId))
				{
					if (AController* KillerController = KillerSnapshot->Controller.Get())
					{
						if (Config.ExperienceReward > 0)
						{
							ExperienceByController.FindOrAdd(TWeakObjectPtr<AController>(KillerController)) += Config.ExperienceReward;
						}
						if (Config.DropGroup.DataTable && !Config.DropGroup.RowName.IsNone())
						{
							FASPendingMonsterDrop& PendingDrop = PendingDrops.AddDefaulted_GetRef();
							PendingDrop.DropGroup = Config.DropGroup;
							PendingDrop.Location = Transforms[EntityIt].GetTransform().GetLocation();
							PendingDrop.KillerController = KillerController;
						}
					}
				}
			}
			Behaviors[EntityIt].State = EASMassMonsterState::Dead;
			Targets[EntityIt].Reset();
			Context.Defer().AddTag<FASMassMonsterDeadTag>(Context.GetEntity(EntityIt));
			++NewlyDeadCount;
		}
	});

	if (NewlyDeadCount > 0)
	{
		if (UWorld* World = GetWorld())
		{
			if (AASGameMode* GameMode = World->GetAuthGameMode<AASGameMode>())
			{
				// Mass 실행 중에는 밤 종료가 Entity Fragment를 즉시 변경할 수 없으므로 다음 틱에 집계한다.
				World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
					GameMode, [GameMode, NewlyDeadCount, ExperienceByController = MoveTemp(ExperienceByController),
						PendingDrops = MoveTemp(PendingDrops)]()
					{
						GameMode->OnEnemiesKilled(NewlyDeadCount);
						for (const TPair<TWeakObjectPtr<AController>, int32>& Entry : ExperienceByController)
						{
							AController* KillerController = Entry.Key.Get();
							AASPlayerState* PlayerState = KillerController
								? KillerController->GetPlayerState<AASPlayerState>() : nullptr;
							if (UASProgressionComponent* Progression = PlayerState ? PlayerState->GetProgressionComponent() : nullptr)
							{
								Progression->AddExperience(Entry.Value);
							}
						}
						if (UASMonsterDropSubsystem* DropSubsystem = GameMode->GetWorld()->GetSubsystem<UASMonsterDropSubsystem>())
						{
							for (const FASPendingMonsterDrop& PendingDrop : PendingDrops)
							{
								DropSubsystem->SpawnDrops(PendingDrop.DropGroup, PendingDrop.Location,
									PendingDrop.KillerController.Get());
							}
						}
					}));
			}
		}
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	DeadQuery.ForEachEntityChunk(Context, [DeltaTime](FMassExecutionContext& Context)
	{
		const TArrayView<FASMassMonsterHealthFragment> Healths = Context.GetMutableFragmentView<FASMassMonsterHealthFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterHealthFragment& Health = Healths[EntityIt];
			if (!Health.bDeathTimerStarted)
			{
				continue;
			}

			Health.DeathTimeRemaining = FMath::Max(0.0f, Health.DeathTimeRemaining - DeltaTime);
			if (Health.DeathTimeRemaining <= 0.0f)
			{
				Context.Defer().AddTag<FASMassMonsterPendingDestroyTag>(Context.GetEntity(EntityIt));
			}
		}
	});

	PendingDestroyQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			Context.Defer().DestroyEntity(Context.GetEntity(EntityIt));
		}
	});
}
