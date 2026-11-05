#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASLootWidget.generated.h"

class UASInventoryComponent;
class UASItemInstance;
class UASLootEntryWidget;
class UButton;
class UCanvasPanelSlot;
class UPanelWidget;
class UTextBlock;
class AASLootChest;
class UASInventoryViewModel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASLootClosedSignature);

/** Displays a loot inventory and transfers clicked items to a player inventory. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASLootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UASLootWidget(const FObjectInitializer& ObjectInitializer);
	UFUNCTION(BlueprintPure, Category="Loot|ViewModel")
	UASInventoryViewModel* GetViewModel() const { return ViewModel; }
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void SetLootContext(UASInventoryComponent* InLootInventory, UASInventoryComponent* InPlayerInventory);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void RefreshLoot();

	UPROPERTY(BlueprintAssignable, Category = "Loot")
	FASLootClosedSignature OnLootClosed;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;

private:
	void BindLootInventory();
	void UnbindLootInventory();
	void RefreshLootHeader();

	UPROPERTY(Transient)
	TObjectPtr<UASInventoryViewModel> ViewModel;
	int32 RenderedSlotCount = INDEX_NONE;

	UFUNCTION()
	void HandleRelicClaimClicked();

	void RefreshRelicRewardButton();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true", ExposeOnSpawn = "true"), Category = "Loot")
	TObjectPtr<UASInventoryComponent> LootInventory;

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true", ExposeOnSpawn = "true"), Category = "Loot")
	TObjectPtr<UASInventoryComponent> PlayerInventory;

	UPROPERTY(EditDefaultsOnly, Category = "Loot")
	TSubclassOf<UASLootEntryWidget> EntryWidgetClass;
	UPROPERTY()
	TSoftClassPtr<UASLootEntryWidget> DefaultEntryWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="Loot|Layout", meta=(ClampMin="1", ClampMax="12"))
	int32 Columns = 4;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UPanelWidget> ItemList;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UButton> CloseButton;
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UButton> HeaderCloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RelicClaimButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RelicClaimText;

	bool bRelicClaimButtonGenerated = false;

	/** Transparent hit area behind LootPanel so outside mouse input reaches this widget. */
	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UButton> OutsideCloseButton;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> LootTitleText;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> ItemCountText;

	UPROPERTY(BlueprintReadOnly, Category="UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> LootPanelSlot;

	TWeakObjectPtr<AASLootChest> LootChest;
};
