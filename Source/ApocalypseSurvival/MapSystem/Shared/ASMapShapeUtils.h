#pragma once

#include "CoreMinimal.h"

class AActor;
class UShapeComponent;

/** 구역과 수풀이 공유하는 포함 판정 헬퍼. 충돌이나 오버랩 이벤트 없이 동작한다. */
namespace ASMapShapeUtils
{
	/**
	 * 위치가 박스, 구, 캡슐 안에 있으면 true를 반환한다.
	 * 캡슐은 수직 원기둥으로 처리한다 (반지름과 절반 높이, 둥근 끝부분 제외).
	 * @param bIgnoreHeight 도형을 무한 수직 기둥으로 처리한다 (단층 맵).
	 */
	APOCALYPSESURVIVAL_API bool ShapeContainsLocation(const UShapeComponent* Shape, const FVector& Location, bool bIgnoreHeight);

	/** 액터의 영역을 정의하는 도형 컴포넌트 ("NotArea" 태그가 있는 컴포넌트는 제외). */
	APOCALYPSESURVIVAL_API void GatherAreaShapes(const AActor* Actor, TArray<TWeakObjectPtr<UShapeComponent>>& OutShapes);

	/** 파괴된 도형은 제외한다. */
	APOCALYPSESURVIVAL_API bool AnyShapeContainsLocation(const TArray<TWeakObjectPtr<UShapeComponent>>& Shapes, const FVector& Location, bool bIgnoreHeight);

	/** 아직 존재하는 도형의 월드 XY 경계 사각형. */
	APOCALYPSESURVIVAL_API void GetFootprints(const TArray<TWeakObjectPtr<UShapeComponent>>& Shapes, TArray<FBox2D>& OutFootprints);

	/** 도형을 보이지 않고 충돌이 없는 영역 표시자로 설정한다. */
	APOCALYPSESURVIVAL_API void ConfigureAreaShape(UShapeComponent* Shape, const FColor& EditorColor);
}
