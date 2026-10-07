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

/** 공격 사거리 Condition 의 인스턴스 데이터. */
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
 * 타깃이 공격 사거리 안에 있는지 검사한다.
 * 사거리 규칙은 UASMonsterCombatComponent::IsTargetInAttackRange() 하나뿐이고 여기서 다시 계산하지 않는다.
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
