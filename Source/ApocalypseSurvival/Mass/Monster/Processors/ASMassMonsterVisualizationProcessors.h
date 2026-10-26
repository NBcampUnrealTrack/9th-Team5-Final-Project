#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityQuery.h"
#include "MassLODCollectorProcessor.h"
#include "MassProcessor.h"
#include "MassRepresentationProcessor.h"
#include "MassVisualizationLODProcessor.h"
#include "ASMassMonsterVisualizationProcessors.generated.h"

/** 일반 Mass 몬스터의 Viewer 거리와 Frustum 정보를 수집한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterLODCollectorProcessor : public UMassLODCollectorProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterLODCollectorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
};
/** 일반 Mass 몬스터의 거리·가시성 기반 Representation LOD를 계산한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterVisualizationLODProcessor : public UMassVisualizationLODProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterVisualizationLODProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
};

/** 일반 Mass 몬스터의 Representation 상태와 ISM Instance를 갱신한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterVisualizationProcessor : public UMassVisualizationProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterVisualizationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	/** Super::Execute 직전에 안개 밖 Entity의 LOD만 임시로 Off로 바꾼다. */
	FMassEntityQuery FogVisibilityQuery;
};
