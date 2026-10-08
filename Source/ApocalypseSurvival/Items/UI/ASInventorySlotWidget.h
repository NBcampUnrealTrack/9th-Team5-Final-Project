#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "ASInventorySlotWidget.generated.h"

class UASInventoryBarWidget;
class UASInventoryComponent;
class UASItemInstance;
class UBorder;
class UImage;
class UTextBlock;

UCLASS()
class APOCALYPSESURVIVAL_API UASInventorySlotDragDrop : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UASInventoryComponent> Inventory;
	int32 FromSlot = INDEX_NONE;
	FGuid ItemId;
};

UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeSlot(UASInventoryBarWidget* InBar, int32 InSlotIndex);
	void RefreshSlot();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& Event, UDragDropOperation* Operation) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBorder;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EquippedText;

private:
	UASItemInstance* GetItem() const;
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryBarWidget> Bar;
	int32 SlotIndex = INDEX_NONE;
	FGuid PressedItem;
	bool bPressed = false;
};
