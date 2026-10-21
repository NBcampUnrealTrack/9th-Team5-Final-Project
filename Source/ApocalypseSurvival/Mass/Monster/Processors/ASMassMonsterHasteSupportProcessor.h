#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterHasteSupportProcessor.generated.h"

/** Mass 일반 몬스터의 이속 지원 파동과 지속시간을 처리한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterHasteSupportProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterHasteSupportProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery HasteSourceQuery;
	FMassEntityQuery ModifierQuery;
};
