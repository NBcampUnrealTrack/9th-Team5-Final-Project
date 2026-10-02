#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASLootWidget.generated.h"

class UASInventoryComponent;
class UASItemInstance;
class UASLootEntryWidget;
class UButton;
class UPanelWidget;

/** Displays a loot inventory and transfers clicked items to a player inventory. */
UCLASS(Blueprintable)
class APOCALYPSESURVIAL_API UASLootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void SetLootContext(UASInventoryComponent* InLootInventory, UASInventoryComponent* InPlayerInventory);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void RefreshLoot();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void BindLootInventory();
	void UnbindLootInventory();

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
};
