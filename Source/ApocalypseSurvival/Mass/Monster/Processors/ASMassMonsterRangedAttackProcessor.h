#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterRangedAttackProcessor.generated.h"

/** Attack 상태인 원거리 일반 몬스터의 투사체 발사와 개체별 쿨다운을 처리한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterRangedAttackProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterRangedAttackProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
