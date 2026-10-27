#include "Core/UI/SASLoadingScreen.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SASLoadingScreen::Construct(const FArguments& InArgs)
{
	StartTime = FPlatformTime::Seconds();
	ForceVolatile(true);
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::Black)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("로딩중")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 32))
				.ColorAndOpacity(FLinearColor::White)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 0.0f).VAlign(VAlign_Center)
			[
				// Reserve the dots' width so the label does not shift during animation.
				SNew(SBox).WidthOverride(80.0f)
				[
					SNew(STextBlock)
					.Text(this, &SASLoadingScreen::GetDotsText)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 32))
					.ColorAndOpacity(FLinearColor::White)
				]
			]
		]
	];
}

int32 SASLoadingScreen::GetVisibleDotCount(const double ElapsedSeconds)
{
	return FMath::FloorToInt(FMath::Max(0.0, ElapsedSeconds) / 0.5) % 4;
}

FText SASLoadingScreen::GetDotsText() const
{
	static const TCHAR* Dots[] = { TEXT(""), TEXT("."), TEXT(". ."), TEXT(". . .") };
	return FText::FromString(Dots[GetVisibleDotCount(FPlatformTime::Seconds() - StartTime)]);
}
