#pragma once

#include "CoreMinimal.h"
#include "Items/UI/ASInventoryBarWidget.h"
#include "ASInventoryPanelWidget.generated.h"

class UTextBlock;
class UButton;

/** Equipment above storage, with the same ten quick slots repeated below. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASInventoryPanelWidget : public UASInventoryBarWidget
{
	GENERATED_BODY()
public:
	UASInventoryPanelWidget(const FObjectInitializer& ObjectInitializer);
	virtual void RefreshSlots() override;

protected:
	virtual void BuildSlotWidgets() override;
	virtual UWidget* GetInventoryBounds() const override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	UFUNCTION()
	void Close();
	UFUNCTION()
	void OpenCrafting();

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UBorder> WindowFrame;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UUniformGridPanel> QuickSlotsPanel;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UUniformGridPanel> EquipmentPanel;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> CapacityText;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> FeedbackText;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UButton> CloseButton;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UButton> CraftingButton;
	UPROPERTY(EditDefaultsOnly, Category="Inventory|Layout", meta=(ClampMin="1", ClampMax="16"))
	int32 StorageColumns = 8;
	int32 RenderedStorageCount = INDEX_NONE;
};
