#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/Data/ASItemGrant.h"
#include "ASCraftingRecipeDefinition.generated.h"

class FDataValidationContext;

/** Fixed recipe. Item combat and presentation data remain on the result's item definition. */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASCraftingRecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	FName RecipeId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	FText DisplayName;

	/** Repeated material entries are added together when checking and consuming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TArray<FASItemGrant> Ingredients;

	/** Ordinary inventory item and quantity per craft. Stackable results may span several stacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	FASItemGrant Result;

	bool ValidateRecipe(FString& OutError) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
