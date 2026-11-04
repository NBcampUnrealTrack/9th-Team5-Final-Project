#include "Characters/Enemy/AI/StateTree/ASMonsterStateTreeConditions.h"

#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "GameFramework/Actor.h"
#include "StateTreeExecutionContext.h"

bool FASMonsterHasValidTargetCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UASMonsterTargetingComponent* Targeting = InstanceData.TargetingComponent;
	if (!IsValid(Targeting) || !Targeting->GetOwner() || !Targeting->GetOwner()->HasAuthority())
	{
		return false;
	}

	// 타깃 거리·생존 판정은 AIController가 주기적으로 갱신한다. 여기서는 그 결과만 읽는다.
	return Targeting->HasValidTarget() != InstanceData.bInvert;
}

bool FASMonsterTargetInAttackRangeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UASMonsterCombatComponent* Combat = InstanceData.CombatComponent;
	const UASMonsterTargetingComponent* Targeting = InstanceData.TargetingComponent;
	if (!IsValid(Combat) || !IsValid(Targeting) || !Combat->GetOwner() || !Combat->GetOwner()->HasAuthority())
	{
		return false;
	}

	// 타깃은 평가 순간의 값을 TargetingComponent 에서 직접 읽는다.
	const AActor* Target = Targeting->GetTargetActor();
	if (!IsValid(Target))
	{
		return false;
	}

	return Combat->CanRequestAttack(Target) != InstanceData.bInvert;
}

bool FASMonsterHealthCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UASMonsterCombatComponent* Combat = InstanceData.CombatComponent;

	if (!IsValid(Combat) || !Combat->GetOwner() || !Combat->GetOwner()->HasAuthority())
	{
		return false;
	}

	const bool bIsBelowThreshold = (Combat->GetHealthPercent() <= InstanceData.HealthThreshold);
	return bIsBelowThreshold != InstanceData.bInvert;
}

namespace
{
	const AActor* ResolvePatternTarget(const AActor* ExplicitTarget,
		const UASMonsterTargetingComponent* TargetingComponent)
	{
		return IsValid(ExplicitTarget)
			? ExplicitTarget
			: (IsValid(TargetingComponent) ? TargetingComponent->GetTargetActor() : nullptr);
	}
}

bool FASMonsterCanRequestPatternByIdCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UASMonsterCombatComponent* Combat = InstanceData.CombatComponent;
	const AActor* Target = ResolvePatternTarget(InstanceData.TargetActor, InstanceData.TargetingComponent);
	if (!IsValid(Combat) || !Combat->GetOwner() || !Combat->GetOwner()->HasAuthority()
		|| !InstanceData.PatternId.IsValid() || !IsValid(Target))
	{
		return false;
	}

	return Combat->CanRequestPatternById(InstanceData.PatternId, Target) != InstanceData.bInvert;
}

bool FASMonsterCanPreparePatternByIdCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UASMonsterCombatComponent* Combat = InstanceData.CombatComponent;
	const AActor* Target = ResolvePatternTarget(InstanceData.TargetActor, InstanceData.TargetingComponent);
	if (!IsValid(Combat) || !Combat->GetOwner() || !Combat->GetOwner()->HasAuthority()
		|| !InstanceData.PatternId.IsValid() || !IsValid(Target))
	{
		return false;
	}

	return Combat->CanPreparePatternById(InstanceData.PatternId, Target) != InstanceData.bInvert;
}

bool FASMonsterTargetInPatternRangeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UASMonsterCombatComponent* Combat = InstanceData.CombatComponent;
	const AActor* Target = ResolvePatternTarget(InstanceData.TargetActor, InstanceData.TargetingComponent);
	if (!IsValid(Combat) || !Combat->GetOwner() || !Combat->GetOwner()->HasAuthority()
		|| !InstanceData.PatternId.IsValid() || !IsValid(Target))
	{
		return false;
	}

	return Combat->IsTargetInPatternRangeById(InstanceData.PatternId, Target) != InstanceData.bInvert;
}
