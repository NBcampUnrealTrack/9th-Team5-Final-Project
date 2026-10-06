#pragma once

#include "CoreMinimal.h"

namespace ASFogMask
{
	/** 짧은 변 해상도와 월드 비율로 정사각형 셀을 계산한다. X는 U (월드 Y), Y는 V (월드 X), 각 변은 16..2048이다. */
	APOCALYPSESURVIVAL_API FIntPoint ComputeResolution(FVector2D WorldSize, int32 ShortSide);

	/** 표시 알파에 경계 고정 이항 흐림을 적용한다. 반경은 0..8이며 Scratch는 InOut과 별도의 재사용 버퍼이다. */
	APOCALYPSESURVIVAL_API void BlurAlpha(TArray<uint8>& InOut, int32 Width, int32 Height, int32 Radius, TArray<uint8>& Scratch);
}
