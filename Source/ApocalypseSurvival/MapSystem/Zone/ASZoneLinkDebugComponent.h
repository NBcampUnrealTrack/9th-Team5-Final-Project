#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "ASZoneLinkDebugComponent.generated.h"

/**
 * 레벨 편집 중 선택한 구역의 양방향 인접 연결선을 청록색으로 그린다.
 * 스폰 지점은 초록색, 리스폰 지점은 파란색 구와 전방 화살표로 표시한다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASZoneLinkDebugComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UASZoneLinkDebugComponent();
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
};
