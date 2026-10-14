#pragma once

#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;
class UShapeComponent;

/** 구역과 수풀이 공유하는 포함 판정 헬퍼. 충돌이나 오버랩 이벤트 없이 동작한다. */
namespace ASMapShapeUtils
{
	/** 짝홀수 규칙으로 오목/볼록 다각형을 검사한다. 변 위의 점도 내부에 포함한다. */
	APOCALYPSESURVIVAL_API bool PolygonContainsPoint(const TArray<FVector2D>& Polygon, const FVector2D& Point);

	/** 점 3개 이상, 면적 1 cm² 이상, 비인접 변의 교차가 없는 다각형인지 검사한다. */
	APOCALYPSESURVIVAL_API bool ValidatePolygon(const TArray<FVector2D>& Polygon, FString& OutError);

	/** 회전과 스케일을 적용한 박스의 네 월드 XY 모서리. */
	APOCALYPSESURVIVAL_API void MakeBoxOutline(const FTransform& ComponentToWorld, const FVector& UnscaledExtent, TArray<FVector2D>& Out);
	/** 로컬 XY 원에 회전과 스케일을 적용한 월드 XY 외곽선. */
	APOCALYPSESURVIVAL_API void MakeCircleOutline(const FTransform& ComponentToWorld, float UnscaledRadius, int32 Segments, TArray<FVector2D>& Out);

	/**
	 * 위치가 다각형, 박스, 구, 캡슐 안에 있으면 true를 반환한다.
	 * 캡슐은 수직 원기둥으로 처리한다 (반지름과 절반 높이, 둥근 끝부분 제외).
	 * @param bIgnoreHeight 도형을 무한 수직 기둥으로 처리한다 (단층 맵).
	 */
	APOCALYPSESURVIVAL_API bool ShapeContainsLocation(const UPrimitiveComponent* Component, const FVector& Location, bool bIgnoreHeight);

	/** 다각형과 도형 컴포넌트를 수집한다 ("NotArea" 태그가 있는 컴포넌트는 제외). */
	APOCALYPSESURVIVAL_API void GatherAreaComponents(const AActor* Actor, TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutComponents);

	/** 파괴된 컴포넌트는 제외한다. 영역은 각 컴포넌트의 합집합이다. */
	APOCALYPSESURVIVAL_API bool AnyShapeContainsLocation(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Components, const FVector& Location, bool bIgnoreHeight);

	/** 월드 XY 외곽선. 닫는 점은 중복하지 않으며 원은 24개 선분으로 근사한다. */
	APOCALYPSESURVIVAL_API void GetAreaOutlines(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Components, TArray<TArray<FVector2D>>& OutOutlines);

	/** 도형을 보이지 않고 충돌이 없는 영역 표시자로 설정한다. */
	APOCALYPSESURVIVAL_API void ConfigureAreaShape(UShapeComponent* Shape, const FColor& EditorColor);
}
