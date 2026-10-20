#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "ASMassMonsterTemporaryVisualProcessor.generated.h"

/** 임시 색상 상태에 맞춰 Mass 몬스터의 ISM Descriptor를 교체한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterTemporaryVisualProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterTemporaryVisualProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
