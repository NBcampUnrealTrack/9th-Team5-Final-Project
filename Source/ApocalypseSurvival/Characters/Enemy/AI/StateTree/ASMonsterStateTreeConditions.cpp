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

	return Combat->IsTargetInAttackRange(Target) != InstanceData.bInvert;
}
