#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "ASZoneLinkDebugComponent.generated.h"

/** 레벨 편집 중 선택한 구역과 양방향으로 연결된 모든 구역까지 청록색 연결선을 그린다. */
UCLASS()
class APOCALYPSESURVIAL_API UASZoneLinkDebugComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UASZoneLinkDebugComponent();
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
};
