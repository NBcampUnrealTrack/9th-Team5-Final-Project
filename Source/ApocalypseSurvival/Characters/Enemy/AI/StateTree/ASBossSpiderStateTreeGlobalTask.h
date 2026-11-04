#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "ASBossSpiderStateTreeGlobalTask.generated.h"

class AASMonsterCharacter;
class UASAbilitySystemComponent;
class UASMonsterCombatComponent;

/** Boss Spider StateTree 전체가 공유하는 설정과 런타임 선택 상태다. */
USTRUCT()
struct FASBossSpiderContextInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AASMonsterCharacter> Monster;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UASAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGameplayTag FanPatternId;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGameplayTag ChargePatternId;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGameplayTag WebTrapPatternId;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGameplayTag EggSummonPatternId;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGameplayTag CeilingSlamPatternId;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGameplayTag CaptureChargePatternId;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bPhaseTwo = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bWebTrapDue = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bMajorPatternDue = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bPreferEggSummon = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasActiveEggs = false;

	UPROPERTY(Transient)
	float DecisionInterval = 0.15f;

	UPROPERTY(Transient)
	float PhaseTwoHealthPercent = 0.60f;

	UPROPERTY(Transient)
	float MajorPatternInterval = 13.0f;

	UPROPERTY(Transient)
	float WebTrapInterval = 8.0f;

	UPROPERTY(Transient)
	double NextDecisionTime = 0.0;

	UPROPERTY(Transient)
	double NextMajorPatternTime = 0.0;

	UPROPERTY(Transient)
	double NextWebTrapTime = 0.0;

	UPROPERTY(Transient)
	uint32 LastObservedPatternStartSerial = 0;
};

/**
 * PatternSet의 Boss Spider 선택 설정을 한 번 해석하고, 성공한 PatternId를 관찰해 StateTree용
 * WebTrap/Major 실행 간격과 Major 교대 상태를 유지한다. 패턴을 직접 선택하거나 실행하지 않는다.
 */
USTRUCT(meta = (DisplayName = "AS Boss Spider Context", Category = "AS|Monster|Boss Spider"))
struct APOCALYPSESURVIVAL_API FASBossSpiderContextGlobalTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FASBossSpiderContextInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

private:
	bool UpdateContext(FInstanceDataType& InstanceData, bool bForceDecisionUpdate) const;
};
