#include "Items/UI/ViewModels/ASCraftingEntryViewModel.h"
#include "Items/UI/ViewModels/ASCraftingViewModel.h"
#include "Items/Data/ASItemDefinition.h"
#include "Engine/Texture2D.h"

bool FASCraftingEntryPresentation::operator==(const FASCraftingEntryPresentation& O) const
{
	return Name.EqualTo(O.Name) && SecondaryText.EqualTo(O.SecondaryText) && Category.EqualTo(O.Category)
		&& Icon == O.Icon && bSelected == O.bSelected && bSatisfied == O.bSatisfied && bEnabled == O.bEnabled;
}

void UASCraftingEntryViewModel::Initialize(UASCraftingViewModel* InOwner, UASItemDefinition* Item, FName InRecipeId, const FText& Name, const FText& Category)
{
	Owner = InOwner; Definition = Item; RecipeId = InRecipeId;
	FASCraftingEntryPresentation Value; Value.Name = Name; Value.Category = Category;
	UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
	RefreshIcon();
}

void UASCraftingEntryViewModel::Update(const FText& Secondary, bool bSatisfied, bool bEnabled, bool bSelected)
{
	FASCraftingEntryPresentation Value = Presentation;
	Value.SecondaryText = Secondary; Value.bSatisfied = bSatisfied; Value.bEnabled = bEnabled; Value.bSelected = bSelected;
	UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
}

void UASCraftingEntryViewModel::RefreshIcon()
{
	const FSoftObjectPath Path = Definition.IsValid() ? Definition->Icon.ToSoftObjectPath() : FSoftObjectPath();
	if (RequestedIcon != Path)
	{
		RequestedIcon = Path;
		IconRequest.Request(Path, FStreamableDelegate::CreateUObject(this, &ThisClass::RefreshIcon));
	}
	FASCraftingEntryPresentation Value = Presentation;
	Value.Icon = Definition.IsValid() ? Definition->Icon.Get() : nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
	if (Owner.IsValid()) Owner->HandleEntryIconChanged(this);
}

void UASCraftingEntryViewModel::Select()
{
	if (Owner.IsValid() && !RecipeId.IsNone() && Presentation.bEnabled) Owner->SelectRecipe(RecipeId);
}

void UASCraftingEntryViewModel::Release()
{
	Owner.Reset(); Definition.Reset(); IconRequest.Reset(); RequestedIcon.Reset();
}
