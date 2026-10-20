#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterBehaviorProcessor.generated.h"

/** 플레이어 Snapshot을 읽어 일반 몬스터의 상위 행동 상태와 이동 목적지를 결정한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterBehaviorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterBehaviorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
