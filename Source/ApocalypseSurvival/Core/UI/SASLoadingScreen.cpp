#include "Core/UI/SASLoadingScreen.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SThrobber.h"

void SASLoadingScreen::Construct(const FArguments& InArgs)
{
	StartTime = FPlatformTime::Seconds();
	Model = InArgs._Model.IsValid() ? InArgs._Model : MakeShared<FASLoadingScreenModel, ESPMode::ThreadSafe>();
	ForceVolatile(true);
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.012f, 0.018f, 0.028f))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([Model = Model]()
				{
					const FText Title = Model->Read().Title;
					return Title.IsEmpty() ? NSLOCTEXT("ASLoading", "Title", "Loading") : Title;
				})
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
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0, 170, 0, 0)
			[
				SNew(SBox).WidthOverride(560)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
					[
						SNew(STextBlock).Justification(ETextJustify::Center).AutoWrapText(true)
						.Text_Lambda([Model = Model]() { return Model->Read().Message; })
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SThrobber)
						.Visibility_Lambda([Model = Model]() { return Model->Read().Phase == EASLoadingPhase::Failed ? EVisibility::Hidden : EVisibility::Visible; })
					]
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 48)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([Model = Model]() { return Model->Read().Phase == EASLoadingPhase::Failed ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
				[
					SNew(SButton).Text(NSLOCTEXT("ASLoading", "Menu", "Return to menu"))
					.Visibility(InArgs._OnReturnToMenu.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
					.OnClicked_Lambda([Callback = InArgs._OnReturnToMenu]() { Callback.ExecuteIfBound(); return FReply::Handled(); })
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, 48.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("참가 취소")))
				.Visibility(InArgs._OnCancel.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
				.OnClicked_Lambda([OnCancel = InArgs._OnCancel]()
				{
					OnCancel.ExecuteIfBound();
					return FReply::Handled();
				})
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
