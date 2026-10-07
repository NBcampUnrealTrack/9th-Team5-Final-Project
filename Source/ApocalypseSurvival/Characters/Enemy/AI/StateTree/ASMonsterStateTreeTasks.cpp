#include "Characters/Enemy/AI/StateTree/ASMonsterStateTreeTasks.h"

#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "GameFramework/Actor.h"
#include "StateTreeExecutionContext.h"

namespace
{
	/** AI 판단은 서버에서만 한다. 컴포넌트가 없거나 서버가 아니면 false. */
	bool IsServerComponent(const UActorComponent* Component)
	{
		return IsValid(Component) && Component->GetOwner() && Component->GetOwner()->HasAuthority();
	}
}

EStateTreeRunStatus FASMonsterAcquireTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsServerComponent(InstanceData.TargetingComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.TimeSinceSearch = 0.0f;
	return InstanceData.TargetingComponent->AcquireTarget() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FASMonsterAcquireTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsServerComponent(InstanceData.TargetingComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.TimeSinceSearch += DeltaTime;
	if (InstanceData.TimeSinceSearch < InstanceData.SearchInterval)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.TimeSinceSearch = 0.0f;
	return InstanceData.TargetingComponent->AcquireTarget() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FASMonsterRequestAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsServerComponent(InstanceData.CombatComponent) || !IsValid(InstanceData.TargetingComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	// 타깃은 요청 순간의 값을 TargetingComponent 에서 직접 읽는다.
	AActor* Target = InstanceData.TargetingComponent->GetTargetActor();
	if (!IsValid(Target))
	{
		return EStateTreeRunStatus::Failed;
	}

	// 사거리와 활성화 가능 여부는 CombatComponent 와 GAS 가 판단한다. 여기서는 결과만 옮긴다.
	return InstanceData.CombatComponent->TryRequestAttack(Target) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
