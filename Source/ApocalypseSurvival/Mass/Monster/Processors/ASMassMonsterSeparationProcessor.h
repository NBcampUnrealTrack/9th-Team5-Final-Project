#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterSeparationProcessor.generated.h"

/**
 * Mass 회피가 놓친 원형 에이전트 간 침투를 이동 직후 보정한다.
 * 예측 회피를 대체하지 않고, 이동/정지 상태 모두에서 겹침을 막는 마지막 안전망이다.
 */
UCLASS(Config = Game, DefaultConfig)
class APOCALYPSESURVIVAL_API UASMassMonsterSeparationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterSeparationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	/** 에이전트 반지름 합에 더하는 시각적 여유 거리. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Mass|Monster|Separation", meta = (ClampMin = "0.0", Units = "cm"))
	float SeparationPadding = 4.0f;

	/** 밀집 군집에서 연쇄 겹침을 푸는 반복 횟수. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Mass|Monster|Separation", meta = (ClampMin = "1", ClampMax = "8"))
	int32 SolverIterations = 3;

	/** 한 반복에서 순간적으로 이동할 수 있는 최대 거리. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Mass|Monster|Separation", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxCorrectionPerIteration = 50.0f;
};
