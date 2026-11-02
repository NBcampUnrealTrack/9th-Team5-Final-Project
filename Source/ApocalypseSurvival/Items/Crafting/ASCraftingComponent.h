#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASCraftingComponent.generated.h"

class UASCraftingRecipeDefinition;
class UASCraftingCatalog;
class UASInventoryComponent;
class UASCraftingWidget;

UENUM(BlueprintType)
enum class EASCraftingResult : uint8
{
	Success,
	UnknownRecipe,
	InvalidRecipe,
	InvalidPlayer,
	MissingMaterials,
	InventoryFull
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FASCraftingFinishedSignature, FName, RecipeId, EASCraftingResult, Result);

/** Controller-owned fixed recipe catalog and RPC bridge. Clients send only a recipe ID. */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASCraftingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UASCraftingComponent();

	/** Assign once on the player-controller Blueprint; manage recipes in the shared catalog asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UASCraftingCatalog> Catalog;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<UASCraftingRecipeDefinition*> GetAvailableRecipes() const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	UASCraftingRecipeDefinition* FindRecipe(FName RecipeId) const;

	/** Advisory local preview; authority checks everything again when committing. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	EASCraftingResult CanCraft(FName RecipeId) const;

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void RequestCraft(FName RecipeId);

	UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
	void ToggleCrafting();

	UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
	void CloseCrafting();

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	bool IsCraftingOpen() const;

	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FASCraftingFinishedSignature OnCraftingFinished;

	UASInventoryComponent* GetInventory() const;
	static FText GetResultText(EASCraftingResult Result);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool CanPlayerCraft() const;
	UFUNCTION(Server, Reliable)
	void ServerCraft(FName RecipeId);
	UFUNCTION(Client, Reliable)
	void ClientCraftFinished(FName RecipeId, EASCraftingResult Result);

	UPROPERTY(Transient)
	TObjectPtr<UASCraftingWidget> CraftingWidget;
};
