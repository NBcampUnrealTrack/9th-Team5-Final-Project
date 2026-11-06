#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "ASLootEntryWidget.generated.h"

class UASItemInstance;
class UButton;
class UImage;
class UTextBlock;
class UASItemRarityBackground;
class UBorder;
class USizeBox;
class UASLootWidget;
class AASLootChest;
class UASItemSlotViewModel;
class UASInventoryViewModel;

/** A loot drag only requests a discard when an explicit world drop target accepts it. */
UCLASS()
class APOCALYPSESURVIVAL_API UASLootItemDragDrop : public UDragDropOperation
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<UASLootWidget> SourceLoot;
	TWeakObjectPtr<AASLootChest> Chest;
	FGuid ItemId;
	TWeakObjectPtr<UASInventoryViewModel> ViewModel;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASLootItemClickedSignature, UASItemInstance*, ItemInstance);

/** Displays one item instance inside a loot list. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASLootEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void SetItemInstance(UASItemInstance* InItemInstance);
	void SetLootSource(UASLootWidget* InLootWidget);
	UFUNCTION(BlueprintCallable, Category="Loot|ViewModel")
	void SetViewModel(UASItemSlotViewModel* Model);
	UFUNCTION(BlueprintPure, Category="Loot|ViewModel")
	UASItemSlotViewModel* GetViewModel() const { return ViewModel; }

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void RefreshEntry();

	UPROPERTY(BlueprintAssignable, Category = "Loot")
	FASLootItemClickedSignature OnItemClicked;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation) override;

private:

	UFUNCTION()
	void HandleItemButtonClicked();

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true", ExposeOnSpawn = "true"), Category = "Loot")
	TObjectPtr<UASItemInstance> ItemInstance;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UButton> ItemButton;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USizeBox> EntrySize;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UASItemRarityBackground> RarityBackground;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> QuantityText;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UBorder> QuantityBadge;
	void ApplyPresentation();
	UPROPERTY(Transient)
	TObjectPtr<UASItemSlotViewModel> ViewModel;
	TWeakObjectPtr<UASLootWidget> LootSource;
	FGuid PressedItem;
	bool bPressed = false;
};
