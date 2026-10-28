#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

/** Paint after scene post-processing so trajectory dots stay white and have no temporal history. */
class SASGrenadeTrajectoryWidget final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SASGrenadeTrajectoryWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Arguments)
	{
		SetVisibility(EVisibility::HitTestInvisible);
	}

	void SetPoints(TArray<FVector2D> InPoints)
	{
		Points = MoveTemp(InPoints);
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		static const FSlateRoundedBoxBrush Outline(FLinearColor::White, 4.0f);
		static const FSlateRoundedBoxBrush Fill(FLinearColor::White, 3.0f);
		const auto DrawDot = [&](const FVector2D& Position)
		{
			FSlateDrawElement::MakeBox(Elements, LayerId, Geometry.ToPaintGeometry(FVector2D(8.0f),
				FSlateLayoutTransform(Position - FVector2D(4.0f))), &Outline, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
			FSlateDrawElement::MakeBox(Elements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(6.0f),
				FSlateLayoutTransform(Position - FVector2D(3.0f))), &Fill, ESlateDrawEffect::None, FLinearColor::White);
		};
		// Stable spacing and diameter in screen units, including when zoomed out.
		constexpr float Spacing = 18.0f;
		float UntilNextDot = 0.0f;
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			const FVector2D Segment = Points[Index] - Points[Index - 1];
			const float Length = Segment.Size();
			if (Length <= UE_KINDA_SMALL_NUMBER) continue;
			while (UntilNextDot <= Length)
			{
				DrawDot(Points[Index - 1] + Segment * (UntilNextDot / Length));
				UntilNextDot += Spacing;
			}
			UntilNextDot -= Length;
		}
		if (!Points.IsEmpty()) DrawDot(Points.Last());
		return LayerId + 1;
	}

private:
	TArray<FVector2D> Points;
};
