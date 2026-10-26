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

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASLootClosedSignature);

/** Displays a loot inventory and transfers clicked items to a player inventory. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASLootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void SetLootContext(UASInventoryComponent* InLootInventory, UASInventoryComponent* InPlayerInventory);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void RefreshLoot();

	UPROPERTY(BlueprintAssignable, Category = "Loot")
	FASLootClosedSignature OnLootClosed;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;

private:
	void BuildDefaultWidgetTree();
	void BindLootInventory();
	void UnbindLootInventory();
	void RefreshLootHeader();

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleItemClicked(UASItemInstance* ItemInstance);

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true", ExposeOnSpawn = "true"), Category = "Loot")
	TObjectPtr<UASInventoryComponent> LootInventory;

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true", ExposeOnSpawn = "true"), Category = "Loot")
	TObjectPtr<UASInventoryComponent> PlayerInventory;

	UPROPERTY(EditDefaultsOnly, Category = "Loot")
	TSubclassOf<UASLootEntryWidget> EntryWidgetClass;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** Transparent hit area behind LootPanel so outside mouse input reaches this widget. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> OutsideCloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LootTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> LootPanelSlot;

	TWeakObjectPtr<AASLootChest> LootChest;
};
