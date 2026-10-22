#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityQuery.h"
#include "MassLODCollectorProcessor.h"
#include "MassProcessor.h"
#include "MassRepresentationProcessor.h"
#include "MassUpdateISMProcessor.h"
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

/**
 * 엔진 UMassUpdateISMProcessor를 대체한다. DefaultMass.ini에서 엔진 Processor의 자동 등록을 끈다.
 * Transform은 엔진과 동일하게 Batch에 쓰고, 안개 표시 Fragment가 있는 Entity는 같은 순서로
 * Per Instance Custom Data 0번에 Dither 표시 알파를 함께 쓴다. 한 ISM에 Transform과 Custom Data를 서로 다른
 * Processor가 따로 쓰면 Instance 순서가 어긋나므로 반드시 한 Processor가 둘 다 기록해야 한다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterUpdateISMProcessor : public UMassUpdateISMProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterUpdateISMProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
};
