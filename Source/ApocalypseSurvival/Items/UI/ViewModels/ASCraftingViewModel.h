#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameplayTagContainer.h"
#include "Core/Framework/ASGameTypes.h"
#include "Items/Crafting/ASCraftingComponent.h"
#include "ASCraftingViewModel.generated.h"

class UASInventoryComponent;
class UAbilitySystemComponent;
class AASPlayerController;
class AASGameState;
class UASCraftingEntryViewModel;
class UASCraftingRecipeDefinition;
class UTexture2D;

UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASCraftingViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	void Initialize(APlayerController* Controller);
	void Shutdown();
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category="Crafting|ViewModel") void SelectRecipe(FName Id);
	UFUNCTION(BlueprintCallable, Category="Crafting|ViewModel") void Refresh();
	UFUNCTION(BlueprintCallable, Category="Crafting|ViewModel") void Craft();
	UFUNCTION(BlueprintCallable, Category="Crafting|ViewModel") void Close();
	void HandleEntryIconChanged(UASCraftingEntryViewModel* Entry);
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") TArray<TObjectPtr<UASCraftingEntryViewModel>> RecipeEntries;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") TArray<TObjectPtr<UASCraftingEntryViewModel>> MaterialEntries;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") FText SelectedName;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") FText SelectedCategory;
	/** Type-specific item stats. Keep the field name for existing CraftingVM Blueprint bindings. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") FText WeaponStats;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") FText OwnedResultText;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") FText AvailabilityText;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") TObjectPtr<UTexture2D> ResultIcon;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") FName SelectedRecipe;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") FText Details;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") FText Status;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") bool bCanCraft = false;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") bool bAwaitingResult = false;
private:
	void BuildRecipeEntries();
	void BuildMaterialEntries(const UASCraftingRecipeDefinition* Recipe);
	void BindPlayerStateSources();
	void UnbindPlayerStateSources();
	void HandleDeadTagChanged(FGameplayTag Tag, int32 NewCount);
	UFUNCTION() void HandlePawnChanged(APawn* NewPawn);
	UFUNCTION() void HandlePhaseChanged(EGamePhase NewPhase);
	UFUNCTION() void HandleFinished(FName RecipeId, EASCraftingResult Result);
	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<UASCraftingComponent> Crafting;
	TWeakObjectPtr<UASInventoryComponent> Inventory;
	TWeakObjectPtr<AASPlayerController> ObservedController;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	TWeakObjectPtr<AASGameState> ObservedGameState;
	FDelegateHandle DeadTagChangedHandle;
	FName PendingRecipe;
	TWeakObjectPtr<const UASCraftingRecipeDefinition> DisplayedRecipe;
	TArray<int64> RequiredQuantities;
};
