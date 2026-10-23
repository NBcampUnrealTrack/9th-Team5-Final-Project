#include "Items/UI/ASItemRarityBackground.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

class SASItemRarityBackground : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SASItemRarityBackground) {}
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
	void SetRarityColor(const FLinearColor InColor)
	{
		Color = InColor;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
protected:
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(72.0f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, const int32 LayerId, const FWidgetStyle& Style, const bool bParentEnabled) const override
	{
		const FVector2D Size = Geometry.GetLocalSize();
		const FLinearColor Tint = Style.GetColorAndOpacityTint();
		const FLinearColor Dark(0.012f, 0.016f, 0.022f, 1.0f);
		TArray<FSlateGradientStop> Stops;
		Stops.Emplace(FVector2f(0.0f, 0.0f), FMath::Lerp(Dark, Color, 0.025f) * Tint);
		Stops.Emplace(FVector2f(0.0f, Size.Y), FMath::Lerp(Dark, Color, 0.22f) * Tint);
		FSlateDrawElement::MakeGradient(Elements, LayerId, Geometry.ToPaintGeometry(), MoveTemp(Stops), Orient_Horizontal);
		const float LineHeight = FMath::Min(2.0f, static_cast<float>(Size.Y));
		FSlateDrawElement::MakeBox(Elements, LayerId + 1,
			Geometry.ToPaintGeometry(FVector2D(Size.X, LineHeight), FSlateLayoutTransform(FVector2D(0.0f, Size.Y - LineHeight))),
			FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Color * Tint);
		return LayerId + 1;
	}
private:
	FLinearColor Color = FLinearColor::White;
};

void UASItemRarityBackground::SetRarity(const EASLootRarity InRarity)
{
	Rarity = InRarity;
	if (Background) Background->SetRarityColor(GetLootRarityColor(Rarity));
}

void UASItemRarityBackground::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SetRarity(Rarity);
}

TSharedRef<SWidget> UASItemRarityBackground::RebuildWidget()
{
	Background = SNew(SASItemRarityBackground).Visibility(EVisibility::HitTestInvisible);
	Background->SetRarityColor(GetLootRarityColor(Rarity));
	return Background.ToSharedRef();
}

void UASItemRarityBackground::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Background.Reset();
}
