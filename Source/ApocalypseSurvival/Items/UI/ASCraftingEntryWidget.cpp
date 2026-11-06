#include "Items/UI/ASCraftingEntryWidget.h"
#include "Items/UI/ViewModels/ASCraftingEntryViewModel.h"
#include "Items/UI/ASItemViewBinding.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Items/UI/ASMenuStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"

TSharedRef<SWidget> UASCraftingEntryWidget::RebuildWidget()
{
	if (!IsDesignTime() && !bMenuLayoutBuilt && NameText && SecondaryText)
	{
		bMenuLayoutBuilt = true;
		for (UWidget* Widget : {static_cast<UWidget*>(SelectButton), static_cast<UWidget*>(SelectionFrame), static_cast<UWidget*>(NameText), static_cast<UWidget*>(SecondaryText), static_cast<UWidget*>(CategoryText), static_cast<UWidget*>(ItemIcon), static_cast<UWidget*>(IconFallback)})
			if (Widget) Widget->RemoveFromParent();
		UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget = Root;
		if (CategoryText) Root->AddChildToVerticalBox(CategoryText)->SetPadding(FMargin(0.f, 14.f, 0.f, 8.f));
		if (!SelectionFrame) SelectionFrame = WidgetTree->ConstructWidget<UBorder>();
		SelectionFrame->SetPadding(FMargin(12.f));
		if (SelectButton)
		{
			SelectButton->SetContent(SelectionFrame);
			Root->AddChildToVerticalBox(SelectButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		else Root->AddChildToVerticalBox(SelectionFrame)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(); SelectionFrame->SetContent(Row);
		if (ItemIcon)
		{
			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(28.f); Size->SetHeightOverride(28.f); Size->SetContent(ItemIcon);
			Row->AddChildToHorizontalBox(Size)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}
		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(); Row->AddChildToHorizontalBox(Texts)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameText->SetAutoWrapText(true); SecondaryText->SetAutoWrapText(true);
		Texts->AddChildToVerticalBox(NameText);
		Texts->AddChildToVerticalBox(SecondaryText)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}
	return Super::RebuildWidget();
}

void UASCraftingEntryWidget::NativePreConstruct()
{
	if (IsDesignTime() && ViewModel) Refresh();
	Super::NativePreConstruct();
}

void UASCraftingEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ASMenuStyle::StyleButton(SelectButton);
	if (SelectButton)
	{
		// SelectionFrame paints the entire row; only its contents need padding.
		FButtonStyle Style = SelectButton->GetStyle();
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		SelectButton->SetStyle(Style);
		if (UButtonSlot* ContentSlot = SelectionFrame ? Cast<UButtonSlot>(SelectionFrame->Slot) : nullptr)
		{
			ContentSlot->SetPadding(FMargin(0.f));
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ContentSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	ASMenuStyle::StyleText(NameText, 15, ASMenuStyle::Text, true);
	ASMenuStyle::StyleText(SecondaryText, 13, ASMenuStyle::Muted);
	ASMenuStyle::StyleText(CategoryText, 11, ASMenuStyle::Muted);
	if (SelectionFrame) SelectionFrame->SetBrushColor(ASMenuStyle::Surface);
	DefaultColor = ASMenuStyle::Surface; bCapturedColor = true;
	if (SelectButton) SelectButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Select);
	SetViewModel(ViewModel);
}

void UASCraftingEntryWidget::NativeDestruct()
{
	if (SelectButton) SelectButton->OnClicked.RemoveDynamic(this, &ThisClass::Select);
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	Super::NativeDestruct();
}

void UASCraftingEntryWidget::SetViewModel(UASCraftingEntryViewModel* Model)
{
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	ViewModel = Model;
	if (ViewModel) ViewModel->AddFieldValueChangedDelegate(UASCraftingEntryViewModel::FFieldNotificationClassDescriptor::Presentation,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this,
			[this](UObject*, UE::FieldNotification::FFieldId) { Refresh(); }));
	ASItemViewBinding::Set(this, TEXT("EntryVM"), ViewModel);
	Refresh();
}

void UASCraftingEntryWidget::Refresh()
{
	const FASCraftingEntryPresentation P = ViewModel ? ViewModel->Presentation : FASCraftingEntryPresentation();
	if (IsDesignTime() || !ASItemViewBinding::HasView(this))
	{
		if (NameText) NameText->SetText(P.Name);
		if (SecondaryText) SecondaryText->SetText(P.SecondaryText);
		if (CategoryText) CategoryText->SetText(P.Category);
	}
	if (CategoryText) CategoryText->SetVisibility(P.Category.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (SelectButton) SelectButton->SetIsEnabled(P.bEnabled);
	if (SelectionFrame)
	{
		if (!bCapturedColor) { DefaultColor = SelectionFrame->GetBrushColor(); bCapturedColor = true; }
		SelectionFrame->SetBrushColor(P.bSelected ? FLinearColor(.13f, .12f, .075f) : DefaultColor);
	}
	if (SecondaryText) SecondaryText->SetColorAndOpacity(FSlateColor(P.bSatisfied ? ASMenuStyle::Success : ASMenuStyle::Muted));
	if (ItemIcon) { ItemIcon->SetBrushFromTexture(P.Icon); ItemIcon->SetVisibility(P.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (IconFallback) IconFallback->SetVisibility(P.Icon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UASCraftingEntryWidget::Select() { if (ViewModel) ViewModel->Select(); }
