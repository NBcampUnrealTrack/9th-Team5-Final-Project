#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterChargeAttackProcessor.generated.h"

/** Mass 일반 몬스터의 선딜·직선 돌진·단일 타깃 피해를 처리한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterChargeAttackProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterChargeAttackProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
	TMap<FMassEntityHandle, TSet<int32>> HitTargetIdsByEntity;
};
