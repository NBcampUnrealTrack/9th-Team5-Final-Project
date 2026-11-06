#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASCraftingEntryWidget.generated.h"

class UASCraftingEntryViewModel;
class UButton;
class UBorder;
class UImage;
class UTextBlock;

/** Designer-authored recipe button or material row, backed by one entry ViewModel. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASCraftingEntryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Crafting|ViewModel") void SetViewModel(UASCraftingEntryViewModel* Model);
	UFUNCTION(BlueprintPure, Category="Crafting|ViewModel") UASCraftingEntryViewModel* GetViewModel() const { return ViewModel; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	void Refresh();
	UFUNCTION() void Select();
	UPROPERTY(Transient) TObjectPtr<UASCraftingEntryViewModel> ViewModel;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UButton> SelectButton;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UBorder> SelectionFrame;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UImage> ItemIcon;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UTextBlock> IconFallback;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UTextBlock> SecondaryText;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UTextBlock> CategoryText;
	UPROPERTY(EditDefaultsOnly, Category="Crafting|Appearance") FLinearColor SelectedColor = FLinearColor(0.18f, 0.14f, 0.065f, 1);
	UPROPERTY(EditDefaultsOnly, Category="Crafting|Appearance") FLinearColor SatisfiedColor = FLinearColor(0.4f, 0.75f, 0.44f, 1);
	UPROPERTY(EditDefaultsOnly, Category="Crafting|Appearance") FLinearColor MissingColor = FLinearColor(0.95f, 0.36f, 0.3f, 1);
	FLinearColor DefaultColor;
	bool bCapturedColor = false;
	bool bMenuLayoutBuilt = false;
};
