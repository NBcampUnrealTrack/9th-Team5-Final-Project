#include "Items/UI/ASCraftingWidget.h"
#include "Items/UI/ViewModels/ASCraftingViewModel.h"
#include "Items/UI/ASItemViewBinding.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"

void UASCraftingWidget::NativeConstruct()
{
	if (!ViewModel) ViewModel = NewObject<UASCraftingViewModel>(this);
	ViewModel->Initialize(GetOwningPlayer());
	// Authored Construct events can obtain the initialized VM through GetViewModel.
	Super::NativeConstruct();
	ASItemViewBinding::Set(this, TEXT("CraftingVM"), ViewModel);
	if (RecipePicker) RecipePicker->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleSelection);
	if (CraftButton) CraftButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Craft);
	if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Close);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ViewModel->RemoveAllFieldValueChangedDelegates(this);
	using FFields = UASCraftingViewModel::FFieldNotificationClassDescriptor;
	for (UE::FieldNotification::FFieldId Field : {FFields::Recipes, FFields::SelectedRecipe, FFields::Details, FFields::Status, FFields::bCanCraft, FFields::bAwaitingResult})
		ViewModel->AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this,
			[this](UObject*, UE::FieldNotification::FFieldId) { Refresh(); }));
	Refresh();
}

void UASCraftingWidget::NativeDestruct()
{
	if (RecipePicker) RecipePicker->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleSelection);
	if (CraftButton) CraftButton->OnClicked.RemoveDynamic(this, &ThisClass::Craft);
	if (CloseButton) CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::Close);
	if (ViewModel) { ViewModel->RemoveAllFieldValueChangedDelegates(this); ViewModel->Shutdown(); }
	Super::NativeDestruct();
}

void UASCraftingWidget::Refresh()
{
	if (!ViewModel) return;
	TGuardValue<bool> UpdatingView(bUpdatingView, true);
	if (!ASItemViewBinding::HasView(this))
	{
		if (Details) Details->SetText(ViewModel->Details);
		if (Status) Status->SetText(ViewModel->Status);
		if (CraftButton) CraftButton->SetIsEnabled(ViewModel->bCanCraft);
	}
	if (RecipePicker)
	{
		bool bOptionsChanged = RecipePicker->GetOptionCount() != ViewModel->Recipes.Num();
		for (int32 Index = 0; !bOptionsChanged && Index < ViewModel->Recipes.Num(); ++Index)
			bOptionsChanged = RecipePicker->GetOptionAtIndex(Index) != ViewModel->Recipes[Index];
		if (bOptionsChanged)
		{
			RecipePicker->ClearOptions();
			for (const FString& Recipe : ViewModel->Recipes) RecipePicker->AddOption(Recipe);
		}
		RecipePicker->SetIsEnabled(!ViewModel->bAwaitingResult);
		const FString Selected = ViewModel->SelectedRecipe.IsNone() ? FString() : ViewModel->SelectedRecipe.ToString();
		if (RecipePicker->GetSelectedOption() != Selected) RecipePicker->SetSelectedOption(Selected);
	}
}

void UASCraftingWidget::HandleSelection(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (ViewModel && !bUpdatingView) ViewModel->SelectRecipe(FName(*SelectedItem));
}
void UASCraftingWidget::Craft() { if (ViewModel) ViewModel->Craft(); }
void UASCraftingWidget::Close() { if (ViewModel) ViewModel->Close(); }
