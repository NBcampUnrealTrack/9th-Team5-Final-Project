#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterSuicideAttackProcessor.generated.h"

/** Mass 일반 몬스터의 선딜 뒤 범위 자폭을 처리한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterSuicideAttackProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterSuicideAttackProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
