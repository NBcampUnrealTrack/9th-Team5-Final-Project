#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterNavigationProcessor.generated.h"

/**
 * Chase/ReturnHome 상태가 지정한 목적지를 Mass NavMesh 경로로 변환한다.
 * 행동 전환과 타깃 선택은 담당하지 않으며, 동기 경로 탐색 수만 프레임 단위로 제한한다.
 */
UCLASS(Config = Game, DefaultConfig)
class APOCALYPSESURVIVAL_API UASMassMonsterNavigationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterNavigationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	/** 한 프레임에 허용할 동기 NavMesh 경로 탐색 수. 나머지는 다음 프레임으로 이월한다. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Mass|Monster|Navigation", meta = (ClampMin = "1"))
	int32 MaxPathRequestsPerFrame = 16;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Mass|Monster|Navigation", meta = (ClampMin = "1.0", Units = "cm"))
	float CorridorWidth = 600.0f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Mass|Monster|Navigation", meta = (ClampMin = "0.0", Units = "cm"))
	float OffsetFromBoundaries = 10.0f;
};
