#pragma once

#include "CoreMinimal.h"
#include "ASVisionTypes.generated.h"

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASVisionSettings
{
	GENERATED_BODY()

	/** 자체 반경이 0 이하인 시야 소스에 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (ClampMin = "0.0", Units = "cm"))
	float DefaultVisionRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float UpdateHz = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision")
	bool bWallOcclusionByDefault = true;

	/** 수풀 안의 공격자가 노출 상태를 유지하는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (ClampMin = "0.0", Units = "s"))
	float AttackRevealSeconds = 3.0f;

	/** 안개 영역의 짧은 변 해상도. 긴 변은 가로세로 비율에 맞춰 정사각형 셀이 되도록 늘린다 (각 변 16..2048). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 MaskResolution = 512;

	/** 안개 경계의 흐림 반경 (텍셀). 0이면 흐림을 적용하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (ClampMin = "0", ClampMax = "8"))
	int32 FogEdgeBlurTexels = 2;

	/** 클라이언트에서 차폐된 시야 마스크를 그릴 때 시야 소스당 광선 수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (ClampMin = "16", ClampMax = "512"))
	int32 OcclusionRayCount = 96;

	/** 시야 트레이스를 위해 액터 위치에 더하는 높이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (Units = "cm"))
	float TraceHeightOffset = 50.0f;

	/** 벽 표면도 그리도록 벽 충돌 지점 너머로 드러내는 추가 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (ClampMin = "0.0", Units = "cm"))
	float WallRevealPadding = 50.0f;
};
