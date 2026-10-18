#pragma once

#include "CoreMinimal.h"
#include "MassMovableVisualizationTrait.h"
#include "ASMassMonsterVisualizationTrait.generated.h"

/** 일반 Mass 몬스터를 Movable ISM으로 표시하는 프로젝트 전용 Visualization Trait. */
UCLASS(meta = (DisplayName = "AS Mass Monster Visualization"))
class APOCALYPSESURVIVAL_API UASMassMonsterVisualizationTrait : public UMassMovableVisualizationTrait
{
	GENERATED_BODY()

public:
	UASMassMonsterVisualizationTrait();

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
