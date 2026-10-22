#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ASVisionTypes.generated.h"

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASVisionSettings
{
	GENERATED_BODY()

	/** 자체 낮 반경이 0 이하인 시야 소스에 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Rules", meta = (ClampMin = "0.0", Units = "cm"))
	float DayVisionRadius = 1500.0f;

	/** 자체 밤 반경이 0 이하인 시야 소스에 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Rules", meta = (ClampMin = "0.0", Units = "cm"))
	float NightVisionRadius = 900.0f;

	/** 낮과 밤의 시야 반경을 전환하는 시간. 0이면 즉시 적용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Rules", meta = (ClampMin = "0.0", Units = "s"))
	float DayNightBlendSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Rules", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float UpdateHz = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Rules")
	bool bWallOcclusionByDefault = true;

	/** 수풀 안에서 공격하거나 플레이어에게 피해를 받은 대상이 노출 상태를 유지하는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Rules", meta = (ClampMin = "0.0", Units = "s"))
	float AttackRevealSeconds = 3.0f;

	/** 안개 영역의 짧은 변 해상도. 긴 변은 가로세로 비율에 맞춰 정사각형 셀이 되도록 늘린다 (각 변 16..2048). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Fog", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 MaskResolution = 512;

	/** 안개 경계의 흐림 반경 (텍셀). 0이면 흐림을 적용하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Fog", meta = (ClampMin = "0", ClampMax = "8"))
	int32 FogEdgeBlurTexels = 3;

	/** 로컬 플레이어 시야를 마스크에 이만큼 넓게 그린다. 갱신 사이 이동 거리와 경계 흐림을 덮어야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Fog", meta = (ClampMin = "0.0", Units = "cm"))
	float LocalVisionMargin = 200.0f;

	/** 새 마스크로 넘어가는 보간 시간. 갱신 간격(1/UpdateHz)과 같으면 경계가 일정한 속도로 움직인다. 0이면 즉시 적용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Fog", meta = (ClampMin = "0", ClampMax = "2", Units = "s"))
	float FogFadeSeconds = 0.1f;

	/** Mass 표현을 유지하는 시야 경계 바깥쪽 Buffer 폭. Buffer 안의 적은 숨기되 ISM은 유지한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Mass Visibility", meta = (ClampMin = "0.0", Units = "cm"))
	float MassVisibilityBufferDistance = 200.0f;

	/** 보이던 Mass 적이 밝은 시야 경계에 닿기 전에 숨도록 안쪽으로 당기는 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Mass Visibility", meta = (ClampMin = "0.0", Units = "cm"))
	float MassVisibilityExitInsetDistance = 50.0f;

	/** 숨겨진 Mass 적이 다시 나타나려면 밝은 시야 경계에서 이 거리만큼 안쪽에 있어야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Mass Visibility", meta = (ClampMin = "0.0", Units = "cm"))
	float MassVisibilityReentryInsetDistance = 50.0f;

	/** Mass 적이 시야에 재진입할 때 Per Instance Custom Data 표시 알파를 올리는 시간. 이탈은 항상 즉시 숨긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Mass Visibility", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float MassVisibilityFadeInSeconds = 0.12f;

	/** Mass 적이 Buffer 바깥에 계속 있을 때 ISM 인스턴스를 제거하기까지의 지연 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Mass Visibility", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
	float MassVisibilityCullDelaySeconds = 0.2f;

	/** 클라이언트에서 차폐된 시야 마스크를 그릴 때 시야 소스당 광선 수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Trace", meta = (ClampMin = "16", ClampMax = "512"))
	int32 OcclusionRayCount = 96;

	/** 시야 트레이스를 위해 액터 위치에 더하는 높이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Trace", meta = (Units = "cm"))
	float TraceHeightOffset = 50.0f;

	/** 벽 표면도 그리도록 벽 충돌 지점 너머로 드러내는 추가 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float WallRevealPadding = 50.0f;

	/** 시야 벽이 차단하는 트레이스 채널 (프로젝트 충돌 채널 ASVisionWall). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision|Trace")
	TEnumAsByte<ECollisionChannel> VisionWallChannel = ECC_GameTraceChannel1;
};

/** 서버 시작 시간을 공유해 모든 머신에서 같은 낮/밤 전환을 평가한다. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASVisionNightBlend
{
	GENERATED_BODY()

	UPROPERTY()
	float FromAlpha = 0.0f;

	UPROPERTY()
	float ToAlpha = 0.0f;

	UPROPERTY()
	double StartServerTime = 0.0;

	/** 월드에 접근하지 않고 주어진 서버 시간의 밤 비율을 계산한다. */
	float Evaluate(double ServerTime, float BlendSeconds) const
	{
		if (BlendSeconds <= 0.0f || ServerTime >= StartServerTime + BlendSeconds)
		{
			return ToAlpha;
		}
		if (ServerTime < StartServerTime)
		{
			return FromAlpha;
		}
		const float T = static_cast<float>((ServerTime - StartServerTime) / BlendSeconds);
		return FMath::Lerp(FromAlpha, ToAlpha, FMath::SmoothStep(0.0f, 1.0f, T));
	}
};
