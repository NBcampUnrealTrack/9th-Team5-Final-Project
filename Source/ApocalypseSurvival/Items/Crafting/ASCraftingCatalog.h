#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ASCraftingCatalog.generated.h"

class UASCraftingRecipeDefinition;
class FDataValidationContext;

/** Shared, designer-authored recipe list. Controllers reference this asset once. */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASCraftingCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TArray<TObjectPtr<UASCraftingRecipeDefinition>> Recipes;

	/** Missing and ambiguous IDs are rejected, including repeated references to the same recipe. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	UASCraftingRecipeDefinition* FindRecipe(FName RecipeId) const;

	/** Keeps catalog order while excluding empty entries and ambiguous IDs. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<UASCraftingRecipeDefinition*> GetAvailableRecipes() const;

	bool ValidateCatalog(FString& OutError) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
