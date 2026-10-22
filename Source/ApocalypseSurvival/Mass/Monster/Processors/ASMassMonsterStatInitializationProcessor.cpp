#include "Mass/Monster/Processors/ASMassMonsterStatInitializationProcessor.h"

#include "Characters/Enemy/Data/ASMonsterStatDefinition.h"
#include "Core/Framework/ASGameState.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Processors/ASMassMonsterChargeAttackProcessor.h"
#include "Mass/Monster/Processors/ASMassMonsterMeleeAttackProcessor.h"
#include "Mass/Monster/Processors/ASMassMonsterNavigationProcessor.h"
#include "Mass/Monster/Processors/ASMassMonsterRangedAttackProcessor.h"
#include "Mass/Monster/Processors/ASMassMonsterSuicideAttackProcessor.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "MassCommonTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterStatInitializationProcessor)

UASMassMonsterStatInitializationProcessor::UASMassMonsterStatInitializationProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	// 전투/이동 Processor가 첫 Tick부터 Resolver 결과만 읽도록 초기화 순서를 명시한다.
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterNavigationProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterMeleeAttackProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterRangedAttackProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterChargeAttackProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterSuicideAttackProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterStatInitializationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterRuntimeStatsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
}

void UASMassMonsterStatInitializationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FASMonsterStatResolveContext StatContext;
	if (const AASGameState* GameState = World->GetGameState<AASGameState>())
	{
		// StartDay가 스폰보다 먼저 CurrentDay를 설정하므로 낮과 밤의 스폰이 같은 라운드를 공유한다.
		StatContext.Round = FMath::Max(1, GameState->GetCurrentDay());
		StatContext.Difficulty = GameState->GetMonsterStatDifficulty();
	}

	EntityQuery.ForEachEntityChunk(Context, [StatContext](FMassExecutionContext& Context)
	{
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();
		const TArrayView<FASMassMonsterHealthFragment> Healths = Context.GetMutableFragmentView<FASMassMonsterHealthFragment>();
		const TArrayView<FASMassMonsterRuntimeStatsFragment> RuntimeStats = Context.GetMutableFragmentView<FASMassMonsterRuntimeStatsFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterRuntimeStatsFragment& RuntimeStat = RuntimeStats[EntityIt];
			if (RuntimeStat.bInitialized)
			{
				continue;
			}

			RuntimeStat.MaxHealth = Config.MaxHealth;
			RuntimeStat.MoveSpeed = Config.MoveSpeed;
			RuntimeStat.AttackPower = Config.AttackPower;
			if (const UASMonsterStatDefinition* StatDefinition = Config.StatDefinition)
			{
				const FASResolvedMonsterStats ResolvedStats = FASMonsterStatResolver::Resolve(*StatDefinition, StatContext);
				RuntimeStat.MaxHealth = ResolvedStats.GetValue(ASMonsterStatTags::MaxHealth, RuntimeStat.MaxHealth);
				RuntimeStat.MoveSpeed = ResolvedStats.GetValue(ASMonsterStatTags::MoveSpeed, RuntimeStat.MoveSpeed);
				RuntimeStat.AttackPower = ResolvedStats.GetValue(ASMonsterStatTags::AttackPower, RuntimeStat.AttackPower);
			}

			// 신규 스폰은 항상 계산된 최대 체력으로 시작한다. 전투 중 재계산 정책은 별도 Processor에서 다룬다.
			Healths[EntityIt].CurrentHealth = RuntimeStat.MaxHealth;
			RuntimeStat.AppliedRound = StatContext.Round;
			RuntimeStat.bInitialized = true;
		}
	});
}
