#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "ASMapPolygonAreaComponent.generated.h"

/**
 * 월드 XY 제어점으로 정의하는 다각형 영역. 점을 드래그해 이동하고 Alt+드래그로
 * 점을 추가하며 컨텍스트 메뉴에서 점을 삭제한다. 접선은 무시하고 항상 직선으로
 * 연결한다. "NotArea" 태그를 붙이면 구역과 수풀의 영역에서 제외한다.
 * 에디터의 스플라인 선 색상 속성으로 구역과 수풀을 구분할 수 있다.
 */
UCLASS(ClassGroup = (Map), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASMapPolygonAreaComponent : public USplineComponent
{
	GENERATED_BODY()

public:
	UASMapPolygonAreaComponent();

	/** 높이를 무시하지 않을 때 컴포넌트 월드 Z를 기준으로 적용하는 절반 높이. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Area", meta = (ClampMin = "0.0", Units = "cm"))
	float HalfHeight = 300.0f;

	/** 닫는 점을 중복하지 않은 월드 XY 제어점. 접선과 제어점 Z는 무시한다. */
	void GetWorldPolygon(TArray<FVector2D>& Out) const;
	bool ContainsLocation(const FVector& Location, bool bIgnoreHeight) const;
	bool ValidateArea(FString& OutError) const;

	virtual void UpdateSpline() override;

private:
	void RefreshWorldPolygon() const;

	mutable TArray<FVector2D> CachedWorldPolygon;
	mutable FTransform CachedWorldTransform = FTransform::Identity;
	mutable bool bWorldPolygonCached = false;
};
