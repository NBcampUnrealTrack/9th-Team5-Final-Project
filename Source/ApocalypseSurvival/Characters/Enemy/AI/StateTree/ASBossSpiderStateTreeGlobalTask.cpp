#include "Characters/Enemy/AI/StateTree/ASBossSpiderStateTreeGlobalTask.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASMonsterPatternSet.h"
#include "Engine/World.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/ASAbilityTags.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FASBossSpiderContextGlobalTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AASMonsterCharacter* Monster = InstanceData.Monster;
	UASMonsterCombatComponent* Combat = InstanceData.CombatComponent;
	UASAbilitySystemComponent* ASC = InstanceData.AbilitySystemComponent;
	if (!IsValid(Monster) || !Monster->HasAuthority() || !IsValid(Combat) || !IsValid(ASC))
	{
		return EStateTreeRunStatus::Failed;
	}

	const UASMonsterDefinition* Definition = Monster->GetMonsterDefinition();
	FASResolvedMonsterPatternSet ResolvedSet;
	const FASBossSpiderStateTreeConfig* Config = Definition && Definition->PatternSet
		&& Definition->PatternSet->ResolvePatternSet(ResolvedSet)
		? ResolvedSet.SelectionPolicyConfig.GetPtr<FASBossSpiderStateTreeConfig>()
		: nullptr;
	if (!Config)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.FanPatternId = Config->FanPatternId;
	InstanceData.ChargePatternId = Config->ChargePatternId;
	InstanceData.WebTrapPatternId = Config->WebTrapPatternId;
	InstanceData.EggSummonPatternId = Config->EggSummonPatternId;
	InstanceData.CeilingSlamPatternId = Config->CeilingSlamPatternId;
	InstanceData.CaptureChargePatternId = Config->CaptureChargePatternId;
	InstanceData.DecisionInterval = FMath::Max(0.05f, Config->DecisionInterval);
	InstanceData.PhaseTwoHealthPercent = FMath::Clamp(Config->PhaseTwoHealthPercent, 0.0f, 1.0f);
	InstanceData.MajorPatternInterval = FMath::Max(0.0f, Config->MajorPatternInterval);
	InstanceData.WebTrapInterval = FMath::Max(0.0f, Config->WebTrapInterval);
	InstanceData.bPreferEggSummon = Config->bPreferEggSummonFirst;
	InstanceData.LastObservedPatternStartSerial = Combat->GetPatternStartSerial();

	const UWorld* World = Monster->GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	InstanceData.NextDecisionTime = CurrentTime;
	InstanceData.NextMajorPatternTime = CurrentTime + InstanceData.MajorPatternInterval;
	InstanceData.NextWebTrapTime = CurrentTime + InstanceData.WebTrapInterval;
	return UpdateContext(InstanceData, true) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FASBossSpiderContextGlobalTask::Tick(FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	return UpdateContext(Context.GetInstanceData(*this), false)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

void FASBossSpiderContextGlobalTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	Context.GetInstanceData(*this) = FInstanceDataType();
}

bool FASBossSpiderContextGlobalTask::UpdateContext(FInstanceDataType& InstanceData,
	const bool bForceDecisionUpdate) const
{
	AASMonsterCharacter* Monster = InstanceData.Monster;
	UASMonsterCombatComponent* Combat = InstanceData.CombatComponent;
	UASAbilitySystemComponent* ASC = InstanceData.AbilitySystemComponent;
	if (!IsValid(Monster) || !Monster->HasAuthority() || !IsValid(Combat) || !IsValid(ASC))
	{
		return false;
	}

	const UWorld* World = Monster->GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	const uint32 PatternStartSerial = Combat->GetPatternStartSerial();
	if (PatternStartSerial != InstanceData.LastObservedPatternStartSerial)
	{
		InstanceData.LastObservedPatternStartSerial = PatternStartSerial;
		const FGameplayTag StartedPatternId = Combat->GetLastStartedPatternId();
		if (StartedPatternId.MatchesTagExact(InstanceData.WebTrapPatternId))
		{
			InstanceData.NextWebTrapTime = CurrentTime + InstanceData.WebTrapInterval;
		}
		else if (StartedPatternId.MatchesTagExact(InstanceData.EggSummonPatternId)
			|| StartedPatternId.MatchesTagExact(InstanceData.CeilingSlamPatternId))
		{
			InstanceData.NextMajorPatternTime = CurrentTime + InstanceData.MajorPatternInterval;
			InstanceData.bPreferEggSummon = !StartedPatternId.MatchesTagExact(InstanceData.EggSummonPatternId);
		}
	}

	if (!bForceDecisionUpdate && CurrentTime < InstanceData.NextDecisionTime)
	{
		return true;
	}
	InstanceData.NextDecisionTime = CurrentTime + InstanceData.DecisionInterval;
	InstanceData.bPhaseTwo = Combat->GetHealthPercent() <= InstanceData.PhaseTwoHealthPercent;
	InstanceData.bWebTrapDue = CurrentTime >= InstanceData.NextWebTrapTime;
	InstanceData.bMajorPatternDue = InstanceData.bPhaseTwo
		&& CurrentTime >= InstanceData.NextMajorPatternTime;
	InstanceData.bHasActiveEggs = ASC->HasMatchingGameplayTag(ASGameplayTags::State_BossSpider_Egg_Active);
	return true;
}
