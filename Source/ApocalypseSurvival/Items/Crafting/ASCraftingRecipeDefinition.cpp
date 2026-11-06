#include "Items/Crafting/ASCraftingRecipeDefinition.h"

#include "Items/Data/ASItemDefinition.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UASCraftingRecipeDefinition::ValidateRecipe(FString& OutError) const
{
	OutError.Reset();
	if (RecipeId.IsNone() || Ingredients.IsEmpty())
	{
		OutError = TEXT("RecipeId and at least one ingredient are required.");
		return false;
	}
	TMap<UASItemDefinition*, int64> Totals;
	for (const FASItemGrant& Ingredient : Ingredients)
	{
		if (!IsValid(Ingredient.ItemDefinition) || Ingredient.Quantity <= 0
			|| Ingredient.ItemDefinition->ItemType != EASItemType::CraftingMaterial
			|| Ingredient.ItemDefinition->MaxStackSize < 1)
		{
			OutError = TEXT("Ingredients require crafting-material definitions and positive quantities/stack sizes.");
			return false;
		}
		int64& Total = Totals.FindOrAdd(Ingredient.ItemDefinition);
		Total += Ingredient.Quantity;
		if (Total > MAX_int32)
		{
			OutError = TEXT("Total ingredient quantity exceeds the supported range.");
			return false;
		}
	}
	if (!IsValid(Result.ItemDefinition) || Result.Quantity != 1
		|| Result.ItemDefinition->ItemType != EASItemType::Weapon || Result.ItemDefinition->MaxStackSize != 1)
	{
		OutError = TEXT("The result must be one weapon with MaxStackSize=1.");
		return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UASCraftingRecipeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateRecipe(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
