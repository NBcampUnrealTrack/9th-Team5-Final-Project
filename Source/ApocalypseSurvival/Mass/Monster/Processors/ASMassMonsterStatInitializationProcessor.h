#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterStatInitializationProcessor.generated.h"

/**
 * 스폰된 Mass 몬스터의 현재 라운드 스탯을 한 번 확정한다.
 * EntityConfig 템플릿은 캐시되므로 Trait의 BuildTemplate에서 라운드를 읽지 않는다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterStatInitializationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterStatInitializationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
