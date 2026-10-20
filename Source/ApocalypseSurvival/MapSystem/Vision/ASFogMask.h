#pragma once

#include "CoreMinimal.h"

namespace ASFogMask
{
	/** 짧은 변 해상도와 월드 비율로 정사각형 셀을 계산한다. X는 U (월드 Y), Y는 V (월드 X), 각 변은 16..2048이다. */
	APOCALYPSESURVIVAL_API FIntPoint ComputeResolution(FVector2D WorldSize, int32 ShortSide);

	/** 삼각형의 부호 있는 Area와 셀 중심 판정으로 Y행의 포함 구간을 구한다. MinX/MaxX와 출력 양 끝은 포함이다.
	 * 수평 경계는 행 전체를 판정하고, 해석적 구간의 양 끝은 원래 변 함수로 보정한다. 빈 구간/거의 0인 Area는 false이다. */
	APOCALYPSESURVIVAL_API bool TriangleRowSpan(const FVector2D& A, const FVector2D& B, const FVector2D& C, double Area,
		int32 Y, int32 MinX, int32 MaxX, int32& OutMinX, int32& OutMaxX);

	/** 별도 Source에서 경계 고정 이항 흐림을 계산해 잘린 Region만 Dest에 쓴다. 반경은 0..8이며 각 패스는 8비트로 반올림한다.
	 * Scratch는 영역 열과 반경만큼 확장한 행의 수평 패스를 보관하는 별도 재사용 버퍼이다. 반경 0은 영역 복사, 잘못된 크기/빈 영역은 무시한다. */
	APOCALYPSESURVIVAL_API void BlurAlphaRegion(const TArray<uint8>& Source, TArray<uint8>& Dest, int32 Width, int32 Height, int32 Radius, const FIntRect& Region, TArray<uint8>& Scratch);

	/** 잘린 Region의 표시 알파를 From에서 Target으로 보간한다. Fraction은 0..1로 제한하고 반올림한다.
	 * 변경 셀의 최소 사각형은 그리드 좌표로 Min 포함, Max 제외이며 반환값은 변경 여부이다.
	 * 잘못된 크기나 빈 영역은 변경 없음으로 보고한다. */
	APOCALYPSESURVIVAL_API bool LerpToward(TArray<uint8>& Shown, const TArray<uint8>& From, const TArray<uint8>& Target, int32 Width, int32 Height, float Fraction, const FIntRect& Region, FIntRect& OutDirty);
}
