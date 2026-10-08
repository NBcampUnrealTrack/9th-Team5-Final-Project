#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ASMonsterStateTreeTasks.generated.h"

class UASMonsterCombatComponent;
class UASMonsterTargetingComponent;

/** 타깃 획득 Task 의 인스턴스 데이터. */
USTRUCT()
struct FASMonsterAcquireTargetInstanceData
{
	GENERATED_BODY()

	/** 몬스터 컨텍스트 Global Task 의 TargetingComponent 를 바인딩한다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	/** 타깃 탐색을 요청하는 간격(초). 매 프레임 탐색하지 않기 위한 값이다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.05", Units = "s"))
	float SearchInterval = 0.35f;

	/** 마지막 탐색 이후 지난 시간. */
	UPROPERTY(Transient)
	float TimeSinceSearch = 0.0f;
};

/**
 * 타깃을 얻을 때까지 SearchInterval 마다 TargetingComponent::AcquireTarget() 을 부른다.
 * 타깃이 잡히면 성공하고, 못 잡았으면 계속 실행한다. 서버가 아니면 실패한다.
 *
 * 누구를 고를지는 직접 정하지 않는다. 선택 규칙은 TargetingComponent 가 전부 갖는다.
 */
USTRUCT(meta = (DisplayName = "AS Monster Acquire Target", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterAcquireTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterAcquireTargetInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** 상태에 들어오자마자 한 번 탐색한다. 이미 타깃이 있으면 바로 성공한다. */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** 간격이 찰 때마다 탐색한다. */
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/** 공격 요청 Task 의 인스턴스 데이터. */
USTRUCT()
struct FASMonsterRequestAttackInstanceData
{
	GENERATED_BODY()

	/** 몬스터 컨텍스트 Global Task 의 CombatComponent 를 바인딩한다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	/** 몬스터 컨텍스트 Global Task 의 TargetingComponent 를 바인딩한다. 타깃은 여기서 직접 읽는다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	/** 패턴 공격을 정상 요청한 뒤 Ability 종료를 기다리는 중인지. */
	UPROPERTY(Transient)
	bool bWaitingForPatternAttack = false;
};

/**
 * 상태에 들어올 때 UASMonsterCombatComponent::TryRequestAttack(Target) 을 한 번 부른다.
 * 타깃은 바인딩된 값이 아니라 TargetingComponent 에서 요청 순간에 직접 읽는다. Global Task 의
 * TargetActor 는 마지막 틱 값이라 뒤처질 수 있기 때문이다.
 * GAS 가 활성화를 받아들였을 때만 성공하고, 사거리 밖이거나 쿨다운 등으로 거절되면 실패한다.
 * 서버가 아니면 실패한다.
 *
 * 피해, 쿨다운, GameplayEffect 는 다루지 않는다. 그건 공격 Ability 와 그 Cooldown Effect 의 일이다.
 *
 * 패턴 몬스터는 State.Monster.Attacking 태그가 사라질 때까지 Running 상태로 기다린 뒤, CombatComponent에
 * 공격 종료를 알린다. 이 시점부터 공용 후딜이 시작된다. 기존 단일 공격 몬스터는 이전처럼 요청 성공 직후 끝낸다.
 */
USTRUCT(meta = (DisplayName = "AS Monster Request Attack", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterRequestAttackTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterRequestAttackInstanceData;

	FASMonsterRequestAttackTask()
	{
		// 패턴 Ability의 선딜·돌진·공격 후딜을 기다린다.
		bShouldCallTick = true;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
