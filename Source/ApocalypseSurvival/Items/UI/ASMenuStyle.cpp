#include "Items/UI/ASMenuStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void ASMenuStyle::StyleText(UTextBlock* Block, int32 Size, FLinearColor Color, bool bBold)
{
	if (!Block) return;
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Font.TypefaceFontName = bBold ? TEXT("Bold") : TEXT("Regular");
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetShadowOffset(FVector2D::ZeroVector);
}

void ASMenuStyle::StyleButton(UButton* Button, bool bPrimary)
{
	if (!Button) return;
	FButtonStyle Style;
	auto Brush = [](FLinearColor Color)
	{
		FSlateBrush Value = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		Value.TintColor = FSlateColor(Color);
		return Value;
	};
	Style.SetNormal(Brush(bPrimary ? FLinearColor(.21f, .18f, .11f) : Surface));
	Style.SetHovered(Brush(bPrimary ? FLinearColor(.31f, .26f, .14f) : FLinearColor(.085f, .093f, .08f)));
	Style.SetPressed(Brush(FLinearColor(.12f, .13f, .10f)));
	Style.SetDisabled(Brush(Background));
	Style.SetNormalForeground(FSlateColor(Text));
	Style.SetHoveredForeground(FSlateColor(Text));
	Style.SetPressedForeground(FSlateColor(Accent));
	Style.SetDisabledForeground(FSlateColor(Muted));
	Style.SetNormalPadding(FMargin(18.f, 11.f));
	Style.SetPressedPadding(FMargin(18.f, 11.f));
	Button->SetStyle(Style);
	Button->SetBackgroundColor(FLinearColor::White);
}

void ASMenuStyle::StyleAuthoredTree(UWidgetTree* Tree)
{
	if (!Tree) return;
	TArray<UWidget*> Widgets;
	Tree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		if (UBorder* Border = Cast<UBorder>(Widget))
		{
			const bool bBackdrop = Border->GetParent() && Border->GetParent()->IsA<UCanvasPanel>();
			Border->SetBrushColor(Border->GetName() == TEXT("OutsideDropArea") ? FLinearColor::Transparent
				: bBackdrop ? FLinearColor(0.f, 0.f, 0.f, .55f) : Surface);
		}
		if (UButton* Button = Cast<UButton>(Widget)) StyleButton(Button);
		if (UTextBlock* Block = Cast<UTextBlock>(Widget))
		{
			const FString Name = Block->GetName();
			const bool bTitle = Name.Contains(TEXT("Title")) || Name == TEXT("SelectedNameText");
			StyleText(Block, bTitle ? 22 : 14, Name.Contains(TEXT("Hint")) ? Muted : Text, bTitle);
		}
	}
}

void ASMenuStyle::StyleWindow(UBorder* Frame)
{
	if (!Frame) return;
	Frame->SetBrushColor(Background);
	Frame->SetPadding(FMargin(0.f));
	if (USizeBox* Size = Cast<USizeBox>(Frame->GetParent()))
	{
		Size->SetWidthOverride(1040.f);
		Size->SetHeightOverride(760.f);
	}
}
