#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "ASMonsterStateTreeConditions.generated.h"

class UASMonsterCombatComponent;
class UASMonsterTargetingComponent;

/** 타깃 유효성 Condition 의 인스턴스 데이터. */
USTRUCT()
struct FASMonsterHasValidTargetInstanceData
{
	GENERATED_BODY()

	/** 몬스터 컨텍스트 Global Task 의 TargetingComponent 를 바인딩한다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	/** true 면 결과를 뒤집는다. "타깃 없음" 조건에 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/**
 * AI Controller가 갱신한 현재 타깃이 있는지 검사한다.
 * 전투 상태를 벗어나는 전이 조건으로 쓴다.
 *
 * 거리·생존 판정과 타깃 제거는 AI Controller가 전담한다. StateTree는 결과를 읽기만 하므로
 * 밤 장거리 추격 중에 LoseTargetRadius를 다시 적용하지 않는다.
 * 서버가 아니면 항상 거짓이다.
 */
USTRUCT(meta = (DisplayName = "AS Monster Has Valid Target", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterHasValidTargetCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterHasValidTargetInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** 현재 거리와 패턴 쿨다운 기준으로 공격 요청이 가능한지 검사하는 Condition의 인스턴스 데이터. */
USTRUCT()
struct FASMonsterTargetInAttackRangeInstanceData
{
	GENERATED_BODY()

	/** 몬스터 컨텍스트 Global Task 의 CombatComponent 를 바인딩한다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	/** 몬스터 컨텍스트 Global Task 의 TargetingComponent 를 바인딩한다. 타깃은 여기서 직접 읽는다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	/** true 면 결과를 뒤집는다. 추격 상태("사거리 밖") 진입 조건에 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/**
 * 타깃에게 지금 공격 요청을 보낼 수 있는지 검사한다.
 * 단일 공격 몬스터는 기존 공격 사거리만, 패턴 몬스터는 거리·개별 쿨다운·공용 후딜을 CombatComponent가 판단한다.
 * 타깃은 바인딩된 값이 아니라 TargetingComponent 에서 평가 순간에 직접 읽는다.
 * 타깃이 없거나 서버가 아니면 항상 거짓이다(뒤집기와 상관없이).
 */
USTRUCT(meta = (DisplayName = "AS Monster Target In Attack Range", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterTargetInAttackRangeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterTargetInAttackRangeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** 체력 비율이 지정한 임계값 이하인지 검사하는 Condition의 인스턴스 데이터. */
USTRUCT()
struct FASMonsterHealthConditionInstanceData
{
	GENERATED_BODY()

	/** 몬스터 컨텍스트의 CombatComponent를 바인딩하여 체력 비율을 조회한다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	/** 비교할 체력 비율 임계값 (예: 0.5 = 50%). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthThreshold = 0.5f;

	/** true면 결과를 뒤집는다. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/**
 * 몬스터의 현재 체력 비율이 임계값 이하인지 검사한다.
 * 페이즈 2 전환 등의 조건에 사용한다.
 */
USTRUCT(meta = (DisplayName = "AS Monster Health Condition", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterHealthCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterHealthConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** PatternId 하나의 현재 실행 가능 여부를 확인하는 Condition 인스턴스 데이터. */
USTRUCT()
struct FASMonsterCanRequestPatternByIdInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	/** 지정하면 TargetingComponent보다 우선한다. Capture Event Payload Target을 바인딩할 때 사용한다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "Ability.Monster.Attack"))
	FGameplayTag PatternId;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/** 거리, 개별 쿨다운, 공용 후딜을 포함한 특정 PatternId의 실행 가능 여부를 CombatComponent에 묻는다. */
USTRUCT(meta = (DisplayName = "AS Monster Can Request Pattern By Id", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterCanRequestPatternByIdCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterCanRequestPatternByIdInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** 공용 후딜·개별 쿨다운·체력 조건을 만족하고, 거리만 맞추면 되는 패턴인지 검사한다. */
USTRUCT(meta = (DisplayName = "AS Monster Can Prepare Pattern By Id", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterCanPreparePatternByIdCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterCanRequestPatternByIdInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** 쿨다운과 무관하게 특정 PatternId의 거리 조건만 확인하는 Condition 인스턴스 데이터. */
USTRUCT()
struct FASMonsterTargetInPatternRangeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "Ability.Monster.Attack"))
	FGameplayTag PatternId;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

USTRUCT(meta = (DisplayName = "AS Monster Target In Pattern Range", Category = "AS|Monster"))
struct APOCALYPSESURVIVAL_API FASMonsterTargetInPatternRangeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASMonsterTargetInPatternRangeInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
