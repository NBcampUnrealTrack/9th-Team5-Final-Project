#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Items/Data/ASItemAssetLoading.h"
#include "ASCraftingEntryViewModel.generated.h"

class UASCraftingViewModel;
class UASItemDefinition;
class UTexture2D;

/** Display data shared by recipe buttons and material rows. */
USTRUCT(BlueprintType)
struct FASCraftingEntryPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText Name;
	UPROPERTY(BlueprintReadOnly) FText SecondaryText;
	UPROPERTY(BlueprintReadOnly) FText Category;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UTexture2D> Icon = nullptr;
	UPROPERTY(BlueprintReadOnly) bool bSelected = false;
	UPROPERTY(BlueprintReadOnly) bool bSatisfied = false;
	UPROPERTY(BlueprintReadOnly) bool bEnabled = true;
	bool operator==(const FASCraftingEntryPresentation& Other) const;
};

UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASCraftingEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	void Initialize(UASCraftingViewModel* InOwner, UASItemDefinition* Item, FName InRecipeId, const FText& Name, const FText& Category = FText::GetEmpty());
	void Update(const FText& Secondary, bool bSatisfied, bool bEnabled, bool bSelected = false);
	void Release();
	UFUNCTION(BlueprintCallable, Category="Crafting|ViewModel") void Select();
	UASItemDefinition* GetDefinition() const { return Definition.Get(); }
	FName GetRecipeId() const { return RecipeId; }
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Crafting|ViewModel") FASCraftingEntryPresentation Presentation;
private:
	void RefreshIcon();
	TWeakObjectPtr<UASCraftingViewModel> Owner;
	TWeakObjectPtr<UASItemDefinition> Definition;
	FName RecipeId;
	FASItemAssetRequest IconRequest;
	FSoftObjectPath RequestedIcon;
};
