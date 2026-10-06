#include "MapSystem/Shared/ASMapShapeUtils.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"

namespace ASMapShapeUtils
{
	static const FName NotAreaTag(TEXT("NotArea"));

	bool ShapeContainsLocation(const UShapeComponent* Shape, const FVector& Location, bool bIgnoreHeight)
	{
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

	void GatherAreaShapes(const AActor* Actor, TArray<TWeakObjectPtr<UShapeComponent>>& OutShapes)
	{
		OutShapes.Reset();
		if (!Actor)
		{
			return;
		}

		TArray<UShapeComponent*> Shapes;
		Actor->GetComponents<UShapeComponent>(Shapes);
		for (UShapeComponent* Shape : Shapes)
		{
			if (Shape && !Shape->ComponentHasTag(NotAreaTag))
			{
				OutShapes.Add(Shape);
			}
		}
	}

	bool AnyShapeContainsLocation(const TArray<TWeakObjectPtr<UShapeComponent>>& Shapes, const FVector& Location, bool bIgnoreHeight)
	{
		for (const TWeakObjectPtr<UShapeComponent>& Shape : Shapes)
		{
			if (ShapeContainsLocation(Shape.Get(), Location, bIgnoreHeight))
			{
				return true;
			}
		}
		return false;
	}

	void GetFootprints(const TArray<TWeakObjectPtr<UShapeComponent>>& Shapes, TArray<FBox2D>& OutFootprints)
	{
		OutFootprints.Reset();
		for (const TWeakObjectPtr<UShapeComponent>& WeakShape : Shapes)
		{
			const UShapeComponent* Shape = WeakShape.Get();
			if (!Shape)
			{
				continue;
			}

			const FBox Bounds = Shape->Bounds.GetBox();
			OutFootprints.Add(FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y)));
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
