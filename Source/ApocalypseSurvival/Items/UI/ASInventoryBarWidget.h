#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASInventoryBarWidget.generated.h"

class UASInventoryComponent;
class UASInventorySlotWidget;
class UUniformGridPanel;
class UBorder;

/** Ten quick slots and a separate weapon position, displayed at the bottom right. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASInventoryBarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetInventory(UASInventoryComponent* InInventory);
	UASInventoryComponent* GetInventory() const { return Inventory; }
	/** Tests the inventory panel, including gaps, rather than the full-screen widget root. */
	bool IsCursorOverInventory() const;
	void ActivateSlot(int32 SlotIndex, FGuid ExpectedItem);
	void DropOneFromSlot(int32 SlotIndex, FGuid ExpectedItem);
	void SwapSlots(int32 FromSlot, int32 ToSlot, FGuid ExpectedFrom, FGuid ExpectedTo);
	void BeginItemDrag();
	void EndItemDrag();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RefreshSlots();

protected:
	virtual void BuildSlotWidgets();
	virtual UWidget* GetInventoryBounds() const;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> SlotsPanel;

	/** Full-screen transparent area, enabled only during item dragging. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> OutsideDropArea;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UASInventorySlotWidget> SlotWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UASInventorySlotWidget>> Slots;

private:
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryComponent> Inventory;
};
