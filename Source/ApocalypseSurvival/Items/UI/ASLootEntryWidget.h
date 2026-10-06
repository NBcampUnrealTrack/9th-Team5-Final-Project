#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASLootEntryWidget.generated.h"

class UASItemInstance;
class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASLootItemClickedSignature, UASItemInstance*, ItemInstance);

/** Displays one item instance inside a loot list. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASLootEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void SetItemInstance(UASItemInstance* InItemInstance);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void RefreshEntry();

	UPROPERTY(BlueprintAssignable, Category = "Loot")
	FASLootItemClickedSignature OnItemClicked;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();

	UFUNCTION()
	void HandleItemButtonClicked();

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true", ExposeOnSpawn = "true"), Category = "Loot")
	TObjectPtr<UASItemInstance> ItemInstance;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;
};
