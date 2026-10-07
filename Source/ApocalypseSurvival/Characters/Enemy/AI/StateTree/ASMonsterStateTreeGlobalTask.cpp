#include "Characters/Enemy/AI/StateTree/ASMonsterStateTreeGlobalTask.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Characters/Enemy/AI/ASMonsterAIController.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	/** 실행 컨텍스트의 소유자에서 AI 컨트롤러를 찾는다. StateTreeAIComponent 는 컨트롤러가 소유한다. */
	AAIController* FindAIController(const FStateTreeExecutionContext& Context)
	{
		UObject* Owner = Context.GetOwner();
		if (AAIController* Controller = Cast<AAIController>(Owner))
		{
			return Controller;
		}

		const UBrainComponent* Brain = Cast<UBrainComponent>(Owner);
		return Brain ? Brain->GetAIOwner() : nullptr;
	}
}

EStateTreeRunStatus FASMonsterContextGlobalTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData = FInstanceDataType();

	AASMonsterAIController* Controller = Cast<AASMonsterAIController>(FindAIController(Context));
	AASMonsterCharacter* Monster = Controller ? Cast<AASMonsterCharacter>(Controller->GetPawn()) : nullptr;

	// AI 판단은 서버에서만 한다. 몬스터를 찾지 못한 경우도 돌릴 수 없으므로 실패로 끝낸다.
	if (!Monster || !Monster->HasAuthority())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.MonsterController = Controller;
	InstanceData.Monster = Monster;
	InstanceData.TargetingComponent = Monster->GetTargetingComponent();
	InstanceData.CombatComponent = Monster->GetCombatComponent();
	InstanceData.AbilitySystemComponent = Monster->GetASAbilitySystemComponent();

	// 루트 상태가 선택되기 전에 출력을 채워 둔다.
	return UpdateTargetOutputs(InstanceData) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FASMonsterContextGlobalTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	// 트리가 활성인 동안 계속 Running 이어야 한다. 몬스터가 사라진 경우에만 실패한다.
	return UpdateTargetOutputs(Context.GetInstanceData(*this)) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void FASMonsterContextGlobalTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// 죽은 몬스터나 파괴된 컴포넌트를 가리키지 않도록 비운다.
	Context.GetInstanceData(*this) = FInstanceDataType();
}

bool FASMonsterContextGlobalTask::UpdateTargetOutputs(FInstanceDataType& InstanceData) const
{
	const AASMonsterCharacter* Monster = InstanceData.Monster;
	UASMonsterTargetingComponent* Targeting = InstanceData.TargetingComponent;

	InstanceData.TargetActor = nullptr;
	InstanceData.DistanceToTarget = 0.0f;
	InstanceData.bHasValidTarget = false;

	if (!IsValid(Monster) || !Monster->HasAuthority() || !IsValid(Targeting))
	{
		return false;
	}

	// 거리·생존 검증은 AIController의 주기 갱신이 전담한다.
	// StateTree가 다시 LoseTargetRadius를 적용하면 밤 장거리 추격 타깃을 지우게 된다.
	if (Targeting->HasValidTarget())
	{
		AActor* Target = Targeting->GetTargetActor();
		InstanceData.TargetActor = Target;
		InstanceData.DistanceToTarget = FVector::Dist2D(Monster->GetActorLocation(), Target->GetActorLocation());
		InstanceData.bHasValidTarget = true;
	}

	return true;
}
