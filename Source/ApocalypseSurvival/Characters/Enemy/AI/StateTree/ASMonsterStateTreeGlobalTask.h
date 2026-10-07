#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ASMonsterStateTreeGlobalTask.generated.h"

class AASMonsterAIController;
class AASMonsterCharacter;
class UASAbilitySystemComponent;
class UASMonsterCombatComponent;
class UASMonsterTargetingComponent;

/** 몬스터 컨텍스트 Global Task 의 인스턴스 데이터. 다른 노드는 Output 값을 바인딩해서 읽는다. */
USTRUCT()
struct FASMonsterContextInstanceData
{
	GENERATED_BODY()

	/** 조종 중인 몬스터. */
	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<AASMonsterCharacter> Monster;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<AASMonsterAIController> MonsterController;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<UASAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * 현재 타깃. 유효하지 않으면 nullptr.
	 * StateTree 바인딩, 디버그 표시, 엔진 Move To Task 용이다. 사거리 판정과 공격 요청은 이 값을 쓰지 않고
	 * TargetingComponent 에서 직접 읽는다. 이 값은 Global Task 의 마지막 갱신 시점 것이라 뒤처질 수 있다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<AActor> TargetActor;

	/** 몬스터와 타깃 중심 사이의 수평 거리(cm). 타깃이 없으면 0. 공격 사거리 판정에는 쓰지 않는다. */
	UPROPERTY(VisibleAnywhere, Category = "Output")
	float DistanceToTarget = 0.0f;

	/** TargetingComponent::ValidateTarget() 를 통과한 타깃이 있는지. */
	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasValidTarget = false;
};

/**
 * 몬스터 StateTree 의 Global Task. 트리가 활성인 동안 계속 실행되며, 몬스터와 컴포넌트 참조,
 * 현재 타깃, 거리를 다른 노드가 바인딩할 수 있게 갱신한다.
 *
 * 루트 상태가 선택되기 전에 EnterState 에서 출력을 한 번 채우므로, 첫 상태 선택부터 값을 쓸 수 있다.
 *
 * 타깃은 찾지 않는다. 이미 잡은 타깃이 여전히 유효한지만 TargetingComponent 에 물어본다.
 * 새 타깃 탐색은 FASMonsterAcquireTargetTask 가 간격을 두고 맡는다.
 * 서버가 아니거나 몬스터를 찾지 못하면 실패하고, 몬스터가 사라지면 다음 틱에 실패한다.
 */
USTRUCT(meta = (DisplayName = "AS Monster Context", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterContextGlobalTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterContextInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** 컨트롤러에서 몬스터와 컴포넌트를 찾아 캐시하고, 타깃 출력을 바로 채운다. */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** 타깃 유효성과 거리를 갱신한다. 플레이어 탐색은 하지 않는다. */
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/** 캐시한 참조와 출력을 모두 비운다. */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	/** 캐시된 참조로 타깃과 거리 출력을 채운다. 갱신할 수 없으면 false. */
	bool UpdateTargetOutputs(FInstanceDataType& InstanceData) const;
};
