#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASCraftingWidget.generated.h"

class UComboBoxString;
class UTextBlock;
class UButton;
class UASCraftingViewModel;

/** View adapter for the designer-authored crafting Widget Blueprint. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASCraftingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Crafting|ViewModel")
	UASCraftingViewModel* GetViewModel() const { return ViewModel; }
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void Refresh();
	UFUNCTION()
	void HandleSelection(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void Craft();
	UFUNCTION()
	void Close();
	UPROPERTY(Transient)
	TObjectPtr<UASCraftingViewModel> ViewModel;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UComboBoxString> RecipePicker;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> Details;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> Status;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UButton> CraftButton;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UButton> CloseButton;
	bool bUpdatingView = false;
};
