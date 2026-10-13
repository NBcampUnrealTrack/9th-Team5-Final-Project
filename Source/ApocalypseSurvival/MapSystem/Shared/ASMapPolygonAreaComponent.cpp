#include "MapSystem/Shared/ASMapPolygonAreaComponent.h"

#include "MapSystem/Shared/ASMapShapeUtils.h"

UASMapPolygonAreaComponent::UASMapPolygonAreaComponent()
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	SetUnselectedSplineSegmentColor(FLinearColor(FColor(80, 200, 255)));
	SetSelectedSplineSegmentColor(FLinearColor(FColor(80, 200, 255)));

	const TArray<FVector> Points = {
		FVector(-1000.0, -1000.0, 0.0), FVector(1000.0, -1000.0, 0.0),
		FVector(1000.0, 1000.0, 0.0), FVector(-1000.0, 1000.0, 0.0)
	};
	SetClosedLoop(true, false);
	SetSplinePoints(Points, ESplineCoordinateSpace::Local, true);
}

void UASMapPolygonAreaComponent::UpdateSpline()
{
	// 뷰포트 편집과 런타임 SetSplinePoints 모두 이 경로를 거친다.
	bWorldPolygonCached = false;
	if (!IsClosedLoop())
	{
		SetClosedLoop(true, false);
	}
	for (int32 Index = 0; Index < GetNumberOfSplinePoints(); ++Index)
	{
		if (GetSplinePointType(Index) != ESplinePointType::Linear)
		{
			SetSplinePointType(Index, ESplinePointType::Linear, false);
		}
	}
	Super::UpdateSpline();
	MarkRenderStateDirty();
}

void UASMapPolygonAreaComponent::RefreshWorldPolygon() const
{
	const FTransform& WorldTransform = GetComponentTransform();
	// 월드에 등록하지 않은 컴포넌트도 변환 변경을 반영한다.
	if (bWorldPolygonCached && CachedWorldTransform.Equals(WorldTransform, 0.0))
	{
		return;
	}

	CachedWorldPolygon.Reset(GetNumberOfSplinePoints());
	for (int32 Index = 0; Index < GetNumberOfSplinePoints(); ++Index)
	{
		const FVector Point = GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
		CachedWorldPolygon.Emplace(Point.X, Point.Y);
	}
	CachedWorldTransform = WorldTransform;
	bWorldPolygonCached = true;
}

void UASMapPolygonAreaComponent::GetWorldPolygon(TArray<FVector2D>& Out) const
{
	RefreshWorldPolygon();
	Out = CachedWorldPolygon;
}

bool UASMapPolygonAreaComponent::ContainsLocation(const FVector& Location, bool bIgnoreHeight) const
{
	if (!bIgnoreHeight && FMath::Abs(Location.Z - GetComponentLocation().Z)
		> HalfHeight * FMath::Abs(GetComponentTransform().GetScale3D().Z))
	{
		return false;
	}
	RefreshWorldPolygon();
	return ASMapShapeUtils::PolygonContainsPoint(CachedWorldPolygon, FVector2D(Location.X, Location.Y));
}

bool UASMapPolygonAreaComponent::ValidateArea(FString& OutError) const
{
	RefreshWorldPolygon();
	return ASMapShapeUtils::ValidatePolygon(CachedWorldPolygon, OutError);
}
