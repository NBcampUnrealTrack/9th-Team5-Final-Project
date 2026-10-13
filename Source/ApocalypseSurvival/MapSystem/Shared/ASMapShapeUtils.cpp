#include "MapSystem/Shared/ASMapShapeUtils.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "MapSystem/Shared/ASMapPolygonAreaComponent.h"

namespace ASMapShapeUtils
{
	static const FName NotAreaTag(TEXT("NotArea"));

	// 위치와 변의 거리를 cm 단위로 비교해 긴 변에서도 같은 경계 허용 오차를 사용한다.
	static bool PointOnSegment(const FVector2D& A, const FVector2D& B, const FVector2D& Point)
	{
		const double Tolerance = UE_KINDA_SMALL_NUMBER;
		const FVector2D Edge = B - A;
		const double LengthSquared = Edge.SizeSquared();
		if (LengthSquared <= FMath::Square(Tolerance))
		{
			return (Point - A).SizeSquared() <= FMath::Square(Tolerance);
		}
		const double Alpha = FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / LengthSquared, 0.0, 1.0);
		return (Point - (A + Alpha * Edge)).SizeSquared() <= FMath::Square(Tolerance);
	}

	static bool SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		if (PointOnSegment(A, B, C) || PointOnSegment(A, B, D)
			|| PointOnSegment(C, D, A) || PointOnSegment(C, D, B))
		{
			return true;
		}
		const double ABC = (B - A) ^ (C - A);
		const double ABD = (B - A) ^ (D - A);
		const double CDA = (D - C) ^ (A - C);
		const double CDB = (D - C) ^ (B - C);
		return ((ABC > 0.0 && ABD < 0.0) || (ABC < 0.0 && ABD > 0.0))
			&& ((CDA > 0.0 && CDB < 0.0) || (CDA < 0.0 && CDB > 0.0));
	}

	bool PolygonContainsPoint(const TArray<FVector2D>& Polygon, const FVector2D& Point)
	{
		if (Polygon.Num() < 3)
		{
			return false;
		}
		bool bInside = false;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FVector2D& A = Polygon[Index];
			const FVector2D& B = Polygon[(Index + 1) % Polygon.Num()];
			if (PointOnSegment(A, B, Point))
			{
				return true;
			}
			// 수평 변은 제외하고 꼭짓점은 한쪽 변에서만 센다.
			if ((A.Y > Point.Y) != (B.Y > Point.Y)
				&& Point.X < A.X + (Point.Y - A.Y) * (B.X - A.X) / (B.Y - A.Y))
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	bool ValidatePolygon(const TArray<FVector2D>& Polygon, FString& OutError)
	{
		OutError.Reset();
		if (Polygon.Num() < 3)
		{
			OutError = TEXT("Polygon needs at least 3 points.");
			return false;
		}
		for (const FVector2D& Point : Polygon)
		{
			if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y))
			{
				OutError = TEXT("Polygon points must be finite.");
				return false;
			}
		}
		for (int32 A = 0; A < Polygon.Num(); ++A)
		{
			const int32 NextA = (A + 1) % Polygon.Num();
			for (int32 B = A + 1; B < Polygon.Num(); ++B)
			{
				const int32 NextB = (B + 1) % Polygon.Num();
				if (NextA == B || NextB == A)
				{
					continue;
				}
				if (SegmentsIntersect(Polygon[A], Polygon[NextA], Polygon[B], Polygon[NextB]))
				{
					OutError = FString::Printf(TEXT("Polygon self-intersection between edges %d and %d."), A, B);
					return false;
				}
			}
		}

		// 큰 월드 좌표의 상쇄 오차를 줄이기 위해 첫 점 기준으로 면적을 합산한다.
		double TwiceArea = 0.0;
		for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
		{
			TwiceArea += (Polygon[Index] - Polygon[0]) ^ (Polygon[Index + 1] - Polygon[0]);
		}
		if (FMath::Abs(TwiceArea) < 2.0)
		{
			OutError = TEXT("Polygon area is near zero (less than 1 cm²).");
			return false;
		}
		return true;
	}

	void MakeBoxOutline(const FTransform& ComponentToWorld, const FVector& UnscaledExtent, TArray<FVector2D>& Out)
	{
		Out.Reset(4);
		const FVector Corners[] = {
			FVector(-UnscaledExtent.X, -UnscaledExtent.Y, 0.0),
			FVector(UnscaledExtent.X, -UnscaledExtent.Y, 0.0),
			FVector(UnscaledExtent.X, UnscaledExtent.Y, 0.0),
			FVector(-UnscaledExtent.X, UnscaledExtent.Y, 0.0)
		};
		for (const FVector& Corner : Corners)
		{
			const FVector World = ComponentToWorld.TransformPosition(Corner);
			Out.Emplace(World.X, World.Y);
		}
	}

	void MakeCircleOutline(const FTransform& ComponentToWorld, float UnscaledRadius, int32 Segments, TArray<FVector2D>& Out)
	{
		Out.Reset();
		if (Segments < 3 || UnscaledRadius <= 0.0f)
		{
			return;
		}
		Out.Reserve(Segments);
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const double Angle = 2.0 * UE_PI * Index / Segments;
			const FVector Local(FMath::Cos(Angle) * UnscaledRadius, FMath::Sin(Angle) * UnscaledRadius, 0.0);
			const FVector World = ComponentToWorld.TransformPosition(Local);
			Out.Emplace(World.X, World.Y);
		}
	}

	bool ShapeContainsLocation(const UPrimitiveComponent* Component, const FVector& Location, bool bIgnoreHeight)
	{
		if (!Component || Component->ComponentHasTag(NotAreaTag))
		{
			return false;
		}
		if (const UASMapPolygonAreaComponent* Polygon = Cast<UASMapPolygonAreaComponent>(Component))
		{
			return Polygon->ContainsLocation(Location, bIgnoreHeight);
		}
		const UShapeComponent* Shape = Cast<UShapeComponent>(Component);
		if (!Shape)
		{
			return false;
		}

		// 로컬 공간에 컴포넌트 스케일이 포함되므로 스케일을 적용하지 않은 크기를 비교한다.
		const FVector Local = Shape->GetComponentTransform().InverseTransformPosition(Location);

		if (const UBoxComponent* Box = Cast<UBoxComponent>(Shape))
		{
			const FVector Extent = Box->GetUnscaledBoxExtent();
			return FMath::Abs(Local.X) <= Extent.X
				&& FMath::Abs(Local.Y) <= Extent.Y
				&& (bIgnoreHeight || FMath::Abs(Local.Z) <= Extent.Z);
		}

		if (const USphereComponent* Sphere = Cast<USphereComponent>(Shape))
		{
			const float Radius = Sphere->GetUnscaledSphereRadius();
			return bIgnoreHeight
				? FVector2D(Local.X, Local.Y).SizeSquared() <= FMath::Square(Radius)
				: Local.SizeSquared() <= FMath::Square(Radius);
		}

		if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Shape))
		{
			const float Radius = Capsule->GetUnscaledCapsuleRadius();
			const float HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
			return FVector2D(Local.X, Local.Y).SizeSquared() <= FMath::Square(Radius)
				&& (bIgnoreHeight || FMath::Abs(Local.Z) <= HalfHeight);
		}

		// 알 수 없는 도형 유형: 월드 경계로 대체한다.
		const FBox Bounds = Shape->Bounds.GetBox();
		return bIgnoreHeight
			? (Location.X >= Bounds.Min.X && Location.X <= Bounds.Max.X && Location.Y >= Bounds.Min.Y && Location.Y <= Bounds.Max.Y)
			: Bounds.IsInsideOrOn(Location);
	}

	void GatherAreaComponents(const AActor* Actor, TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutComponents)
	{
		OutComponents.Reset();
		if (!Actor)
		{
			return;
		}

		TArray<UPrimitiveComponent*> Components;
		Actor->GetComponents<UPrimitiveComponent>(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			if (Component && !Component->ComponentHasTag(NotAreaTag)
				&& (Component->IsA<UShapeComponent>() || Component->IsA<UASMapPolygonAreaComponent>()))
			{
				OutComponents.Add(Component);
			}
		}
	}

	bool AnyShapeContainsLocation(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Components, const FVector& Location, bool bIgnoreHeight)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Component : Components)
		{
			if (ShapeContainsLocation(Component.Get(), Location, bIgnoreHeight))
			{
				return true;
			}
		}
		return false;
	}

	void GetAreaOutlines(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Components, TArray<TArray<FVector2D>>& OutOutlines)
	{
		OutOutlines.Reset();
		for (const TWeakObjectPtr<UPrimitiveComponent>& WeakComponent : Components)
		{
			const UPrimitiveComponent* Component = WeakComponent.Get();
			if (!Component || Component->ComponentHasTag(NotAreaTag))
			{
				continue;
			}

			TArray<FVector2D> Outline;
			if (const UASMapPolygonAreaComponent* Polygon = Cast<UASMapPolygonAreaComponent>(Component))
			{
				Polygon->GetWorldPolygon(Outline);
			}
			else if (const UBoxComponent* Box = Cast<UBoxComponent>(Component))
			{
				MakeBoxOutline(Box->GetComponentTransform(), Box->GetUnscaledBoxExtent(), Outline);
			}
			else if (const USphereComponent* Sphere = Cast<USphereComponent>(Component))
			{
				MakeCircleOutline(Sphere->GetComponentTransform(), Sphere->GetUnscaledSphereRadius(), 24, Outline);
			}
			else if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Component))
			{
				MakeCircleOutline(Capsule->GetComponentTransform(), Capsule->GetUnscaledCapsuleRadius(), 24, Outline);
			}
			else if (Component->IsA<UShapeComponent>())
			{
				// 알 수 없는 도형 유형: 월드 경계 사각형으로 대체한다.
				const FBox Bounds = Component->Bounds.GetBox();
				Outline = {
					FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Min.Y),
					FVector2D(Bounds.Max.X, Bounds.Max.Y), FVector2D(Bounds.Min.X, Bounds.Max.Y)
				};
			}
			else
			{
				continue;
			}
			OutOutlines.Add(MoveTemp(Outline));
		}
	}

	void ConfigureAreaShape(UShapeComponent* Shape, const FColor& EditorColor)
	{
		if (!Shape)
		{
			return;
		}

		Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shape->SetGenerateOverlapEvents(false);
		Shape->SetCanEverAffectNavigation(false);
		Shape->SetHiddenInGame(true);
		Shape->ShapeColor = EditorColor;
	}
}
