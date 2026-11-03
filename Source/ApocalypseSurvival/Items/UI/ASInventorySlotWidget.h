#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "ASInventorySlotWidget.generated.h"

class UASInventoryBarWidget;
class UASInventoryComponent;
class UASItemSlotViewModel;
class UASInventoryViewModel;
class UBorder;
class UImage;
class UTextBlock;
class UASItemRarityBackground;

UCLASS()
class APOCALYPSESURVIVAL_API UASInventorySlotDragDrop : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UASInventoryComponent> Inventory;
	UPROPERTY()
	TObjectPtr<UASInventoryViewModel> ViewModel;
	UPROPERTY()
	TObjectPtr<UASInventoryBarWidget> SourceBar;
	int32 FromSlot = INDEX_NONE;
	FGuid ItemId;
	virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
};

UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeSlot(UASInventoryBarWidget* InBar, int32 InSlotIndex);
	void RefreshSlot();
	UFUNCTION(BlueprintPure, Category="Inventory|ViewModel")
	UASItemSlotViewModel* GetViewModel() const { return ViewModel; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& Event, UDragDropOperation* Operation) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UBorder> SlotBorder;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UASItemRarityBackground> RarityBackground;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UImage> ItemIcon;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> KeyText;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> ItemNameText;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> QuantityText;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> EquippedText;

private:
	void ApplyPresentation();
	UPROPERTY(Transient)
	TObjectPtr<UASItemSlotViewModel> ViewModel;
	UPROPERTY(Transient)
	TObjectPtr<UASInventoryBarWidget> Bar;
	int32 SlotIndex = INDEX_NONE;
	FGuid PressedItem;
	bool bPressed = false;
	bool bCapturedSlotColor = false;
	FLinearColor DefaultSlotColor;
	UPROPERTY(EditDefaultsOnly, Category="Inventory|Appearance")
	FLinearColor EquippedSlotColor = FLinearColor(0.15f, 0.4f, 0.65f, 0.95f);

};
