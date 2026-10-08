#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASInventoryBarWidget.generated.h"

class UASInventoryComponent;
class UASInventorySlotWidget;
class UUniformGridPanel;
class UBorder;

/** The player's inventory: ten permanent positions, displayed at the bottom right. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASInventoryBarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetInventory(UASInventoryComponent* InInventory);
	UASInventoryComponent* GetInventory() const { return Inventory; }
	void ActivateSlot(int32 SlotIndex, FGuid ExpectedItem);
	void SwapSlots(int32 FromSlot, int32 ToSlot, FGuid ExpectedFrom, FGuid ExpectedTo);
	void BeginItemDrag();
	void EndItemDrag();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshSlots();

protected:
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

private:
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryComponent> Inventory;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UASInventorySlotWidget>> Slots;
};
