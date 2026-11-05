#include "Items/Crafting/ASCraftingCatalog.h"

#include "Items/Crafting/ASCraftingRecipeDefinition.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UASCraftingRecipeDefinition* UASCraftingCatalog::FindRecipe(FName RecipeId) const
{
	if (RecipeId.IsNone()) return nullptr;
	UASCraftingRecipeDefinition* Found = nullptr;
	for (UASCraftingRecipeDefinition* Recipe : Recipes)
	{
		if (!IsValid(Recipe) || Recipe->RecipeId != RecipeId) continue;
		if (Found) return nullptr;
		Found = Recipe;
	}
	return Found;
}

TArray<UASCraftingRecipeDefinition*> UASCraftingCatalog::GetAvailableRecipes() const
{
	TArray<UASCraftingRecipeDefinition*> Result;
	TMap<FName, int32> Counts;
	for (const UASCraftingRecipeDefinition* Recipe : Recipes)
		if (IsValid(Recipe) && !Recipe->RecipeId.IsNone()) ++Counts.FindOrAdd(Recipe->RecipeId);
	for (UASCraftingRecipeDefinition* Recipe : Recipes)
		if (IsValid(Recipe) && Counts.FindRef(Recipe->RecipeId) == 1) Result.Add(Recipe);
	return Result;
}

bool UASCraftingCatalog::ValidateCatalog(FString& OutError) const
{
	OutError.Reset();
	TSet<FName> RecipeIds;
	for (const UASCraftingRecipeDefinition* Recipe : Recipes)
	{
		if (!IsValid(Recipe))
		{
			OutError = TEXT("Recipes contains an empty or missing recipe reference.");
			return false;
		}
		FString RecipeError;
		if (!Recipe->ValidateRecipe(RecipeError))
		{
			OutError = FString::Printf(TEXT("Recipe '%s': %s"), *GetNameSafe(Recipe), *RecipeError);
			return false;
		}
		if (RecipeIds.Contains(Recipe->RecipeId))
		{
			OutError = FString::Printf(TEXT("Duplicate RecipeId '%s'. Each catalog entry must have a unique ID."), *Recipe->RecipeId.ToString());
			return false;
		}
		RecipeIds.Add(Recipe->RecipeId);
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UASCraftingCatalog::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateCatalog(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
