#pragma once

#include "CoreMinimal.h"
#include "Items/UI/ASInventoryBarWidget.h"
#include "ASInventoryPanelWidget.generated.h"

class UTextBlock;

/** Equipment above storage, with the same ten quick slots repeated below. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASInventoryPanelWidget : public UASInventoryBarWidget
{
	GENERATED_BODY()
public:
	virtual void RefreshSlots() override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void BuildSlotWidgets() override;
	virtual UWidget* GetInventoryBounds() const override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	UFUNCTION()
	void Close();
	UFUNCTION()
	void OpenCrafting();
	UFUNCTION()
	void ShowActionFailure(FText Message);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowFrame;
	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> QuickSlotsPanel;
	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> EquipmentPanel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CapacityText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeedbackText;
	int32 RenderedStorageCount = INDEX_NONE;
};
