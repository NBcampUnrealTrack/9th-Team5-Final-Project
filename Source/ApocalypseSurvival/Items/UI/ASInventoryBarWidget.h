#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASInventoryBarWidget.generated.h"

class UASInventoryComponent;
class UASInventorySlotWidget;
class UUniformGridPanel;

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

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshSlots();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> SlotsPanel;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UASInventorySlotWidget> SlotWidgetClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryComponent> Inventory;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UASInventorySlotWidget>> Slots;
};
