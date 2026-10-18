#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterMeleeAttackProcessor.generated.h"

/** Attack 상태인 근접 일반 몬스터의 서버 피해와 개체별 쿨다운을 처리한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterMeleeAttackProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterMeleeAttackProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
