#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "Core/Framework/ASGameTypes.h"
#include "Items/Crafting/ASCraftingComponent.h"
#include "ASCraftingWidget.generated.h"

class UComboBoxString;
class UTextBlock;
class UButton;
class UASInventoryComponent;
class UAbilitySystemComponent;
class AASPlayerController;
class AASGameState;

/** Native starter UI: recipe selection, owned/required materials and an authoritative result message. */
UCLASS()
class APOCALYPSESURVIVAL_API UASCraftingWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BindPlayerStateSources();
	void UnbindPlayerStateSources();
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	UFUNCTION()
	void HandlePawnChanged(APawn* NewPawn);
	UFUNCTION()
	void HandlePhaseChanged(EGamePhase NewPhase);
	UFUNCTION()
	void Refresh();
	UFUNCTION()
	void HandleSelection(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void Craft();
	UFUNCTION()
	void Close();
	UFUNCTION()
	void HandleFinished(FName RecipeId, EASCraftingResult Result);

	UPROPERTY(Transient)
	TObjectPtr<UASCraftingComponent> Crafting;
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryComponent> Inventory;
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> RecipePicker;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Details;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Status;
	UPROPERTY(Transient)
	TObjectPtr<UButton> CraftButton;
	bool bAwaitingResult = false;
	TWeakObjectPtr<AASPlayerController> ObservedController;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	TWeakObjectPtr<AASGameState> ObservedGameState;
	FDelegateHandle DeadTagChangedHandle;
};
