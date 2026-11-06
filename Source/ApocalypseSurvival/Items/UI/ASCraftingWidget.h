#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/UI/ASPlayerMenuTabsWidget.h"
#include "ASCraftingWidget.generated.h"

class UPanelWidget;
class UImage;
class UTextBlock;
class UButton;
class UASCraftingViewModel;
class UASCraftingEntryViewModel;
class UASCraftingEntryWidget;

/** Recipe browser, selected weapon preview, and material list authored in WBP_Crafting. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASCraftingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UASCraftingWidget(const FObjectInitializer& ObjectInitializer);
	UFUNCTION(BlueprintPure, Category="Crafting|ViewModel")
	UASCraftingViewModel* GetViewModel() const { return ViewModel; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UFUNCTION() void SelectMenuTab(EASPlayerMenuTab Tab);
	UPROPERTY(Transient) TObjectPtr<UASPlayerMenuTabsWidget> MenuTabs;
	UFUNCTION() void Refresh();
	UFUNCTION() void Craft();
	UFUNCTION() void Close();
	void Populate(UPanelWidget* Panel, const TArray<TObjectPtr<UASCraftingEntryViewModel>>& Entries, TSubclassOf<UASCraftingEntryWidget> Class);
	UPROPERTY(Transient) TObjectPtr<UASCraftingViewModel> ViewModel;
	UPROPERTY(Transient) TArray<TObjectPtr<UASCraftingEntryViewModel>> RenderedRecipes;
	UPROPERTY(Transient) TArray<TObjectPtr<UASCraftingEntryViewModel>> RenderedMaterials;
	UPROPERTY(EditDefaultsOnly, Category="Crafting|UI") TSubclassOf<UASCraftingEntryWidget> RecipeEntryClass;
	UPROPERTY(EditDefaultsOnly, Category="Crafting|UI") TSubclassOf<UASCraftingEntryWidget> MaterialEntryClass;
	UPROPERTY() TSoftClassPtr<UASCraftingEntryWidget> DefaultRecipeEntryClass;
	UPROPERTY() TSoftClassPtr<UASCraftingEntryWidget> DefaultMaterialEntryClass;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UPanelWidget> RecipeList;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UPanelWidget> MaterialList;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UImage> ResultIcon;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UTextBlock> ResultFallback;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UTextBlock> EmptyRecipesText;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true")) TObjectPtr<UButton> CraftButton;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true")) TObjectPtr<UButton> CloseButton;
};
