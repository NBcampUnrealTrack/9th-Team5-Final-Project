#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterDeathProcessor.generated.h"

/** 체력이 소진된 Mass 몬스터를 Dead/PendingDestroy 단계로 보내고 지연 제거한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterDeathProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterDeathProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery NewlyDeadQuery;
	FMassEntityQuery DeadQuery;
	FMassEntityQuery PendingDestroyQuery;
};
