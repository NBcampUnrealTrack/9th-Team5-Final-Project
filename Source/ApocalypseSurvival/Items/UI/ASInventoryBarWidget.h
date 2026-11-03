#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASInventoryBarWidget.generated.h"

class UASInventoryComponent;
class UASInventorySlotWidget;
class UUniformGridPanel;
class UBorder;
class UASInventoryViewModel;

/** Ten quick slots and a separate weapon position, displayed at the bottom right. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASInventoryBarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UASInventoryBarWidget(const FObjectInitializer& ObjectInitializer);
	void SetInventory(UASInventoryComponent* InInventory);
	UFUNCTION(BlueprintPure, Category="Inventory|ViewModel")
	UASInventoryViewModel* GetViewModel() const { return ViewModel; }
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
	UASInventorySlotWidget* CreateSlotWidget(int32 Index);
	virtual void NativePreConstruct() override;
	virtual UWidget* GetInventoryBounds() const;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UUniformGridPanel> SlotsPanel;

	/** Full-screen transparent area, enabled only during item dragging. */
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UBorder> OutsideDropArea;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UASInventorySlotWidget> SlotWidgetClass;

	/** Serialized soft reference keeps the default template cookable without loading it during CDO construction. */
	UPROPERTY()
	TSoftClassPtr<UASInventorySlotWidget> DefaultSlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Inventory|Layout", meta=(ClampMin="1", ClampMax="10"))
	int32 QuickSlotsPerRow = 5;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UASInventorySlotWidget>> Slots;

private:
	void BindViewModel();
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryViewModel> ViewModel;
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryComponent> Inventory;
};
