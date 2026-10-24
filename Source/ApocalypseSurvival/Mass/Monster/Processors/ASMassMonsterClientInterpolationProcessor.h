#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "ASMassMonsterClientInterpolationProcessor.generated.h"

/** 서버 스냅샷 사이의 Transform을 클라이언트에서 보간해 Representation에 전달한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterClientInterpolationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterClientInterpolationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
