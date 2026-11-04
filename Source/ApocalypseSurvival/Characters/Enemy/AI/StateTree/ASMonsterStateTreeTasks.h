#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ASMonsterStateTreeTasks.generated.h"

class UASMonsterCombatComponent;
class UASMonsterTargetingComponent;
class AASBossSpiderWebTrap;
class AASMonsterAIController;

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

/** 특정 PatternId를 명시적으로 요청하는 Task 인스턴스 데이터. */
USTRUCT()
struct FASMonsterRequestPatternByIdInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	/** 지정하면 TargetingComponent보다 우선한다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "Ability.Monster.Attack"))
	FGameplayTag PatternId;

	UPROPERTY(Transient)
	bool bRequestStarted = false;
};

/** PatternId를 한 번 요청하고 CombatComponent가 관찰하는 Ability 종료까지 기다린다. */
USTRUCT(meta = (DisplayName = "AS Monster Request Pattern By Id", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterRequestPatternByIdTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterRequestPatternByIdInstanceData;
	FASMonsterRequestPatternByIdTask() { bShouldCallTick = true; }

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

/** CaptureReady Event Payload를 보관하며 CaptureCharge를 요청하는 Task 인스턴스 데이터. */
USTRUCT()
struct FASBossSpiderRequestCaptureChargeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AASBossSpiderWebTrap> SourceTrap;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "Ability.Monster.Attack"))
	FGameplayTag CaptureChargePatternId;

	UPROPERTY(Transient)
	bool bRequestStarted = false;
};

/** 현재 패턴/후딜이 끝날 때까지 Payload를 유지하고, 유효한 WebBound 대상에게 CaptureCharge를 요청한다. */
USTRUCT(meta = (DisplayName = "AS Boss Spider Request Capture Charge", Category = "AS|Monster|Boss Spider"))
struct APOCALYPSESURVIVAL_API FASBossSpiderRequestCaptureChargeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASBossSpiderRequestCaptureChargeInstanceData;
	FASBossSpiderRequestCaptureChargeTask() { bShouldCallTick = true; }

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

/** 특정 PatternId의 사거리까지 접근하는 Task 인스턴스 데이터. */
USTRUCT()
struct FASMonsterMoveIntoPatternRangeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AASMonsterAIController> MonsterController;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "Ability.Monster.Attack"))
	FGameplayTag PatternId;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.05", Units = "s"))
	float RepathInterval = 0.25f;

	/** 이 시간 동안 유의미한 거리 진전이 없으면 막힌 경로로 판단하고 실패한다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1", Units = "s"))
	float MoveTimeout = 5.0f;

	/** 이동 정지 타이머를 초기화할 최소 누적 진전 거리. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1", Units = "cm"))
	float ProgressTolerance = 5.0f;

	/** 최소 사거리 경계에서 반복적으로 진입/이탈하는 것을 막는 후퇴 여유 거리. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
	float RetreatPadding = 50.0f;

	/** PathFollowing의 도착 오차를 흡수하기 위해 최대 사거리보다 안쪽으로 진입하는 여유 거리. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
	float ApproachPadding = 50.0f;

	/** PathFollowing이 목표 경계 밖에서 도착 처리하는 오차를 감안해 실제 이동 목표를 더 안쪽으로 설정한다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
	float PathArrivalPadding = 25.0f;

	UPROPERTY(Transient)
	float TimeSinceMoveRequest = 0.0f;

	UPROPERTY(Transient)
	float TimeWithoutProgress = 0.0f;

	UPROPERTY(Transient)
	float LastDistanceToDesiredRange = TNumericLimits<float>::Max();

	UPROPERTY(Transient)
	bool bMovementModeInitialized = false;

	UPROPERTY(Transient)
	bool bRetreating = false;
};

/** AIController의 전투 MoveTo와 경쟁하지 않고 StateTree가 선택한 패턴의 사거리 범위로 접근하거나 후퇴한다. */
USTRUCT(meta = (DisplayName = "AS Monster Move Into Pattern Range", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterMoveIntoPatternRangeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterMoveIntoPatternRangeInstanceData;
	FASMonsterMoveIntoPatternRangeTask() { bShouldCallTick = true; }

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
