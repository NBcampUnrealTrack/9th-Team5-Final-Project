#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterSummonProcessor.generated.h"

/** Mass 일반 몬스터가 처리 구간 밖의 지연 Spawn Queue로 증원 Entity를 요청한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterSummonProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterSummonProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
