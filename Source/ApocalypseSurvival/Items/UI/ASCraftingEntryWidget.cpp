#include "Items/UI/ASCraftingEntryWidget.h"
#include "Items/UI/ViewModels/ASCraftingEntryViewModel.h"
#include "Items/UI/ASItemViewBinding.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UASCraftingEntryWidget::NativePreConstruct()
{
	if (IsDesignTime() && ViewModel) Refresh();
	Super::NativePreConstruct();
}

void UASCraftingEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
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
		SelectionFrame->SetBrushColor(P.bSelected ? SelectedColor : DefaultColor);
	}
	if (SecondaryText) SecondaryText->SetColorAndOpacity(FSlateColor(P.bSatisfied ? SatisfiedColor : MissingColor));
	if (ItemIcon) { ItemIcon->SetBrushFromTexture(P.Icon); ItemIcon->SetVisibility(P.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (IconFallback) IconFallback->SetVisibility(P.Icon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UASCraftingEntryWidget::Select() { if (ViewModel) ViewModel->Select(); }
