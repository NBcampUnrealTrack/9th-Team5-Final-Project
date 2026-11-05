#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Items/Data/ASItemAssetLoading.h"
#include "Items/Data/ASItemTypes.h"
#include "ASItemSlotViewModel.generated.h"

class UASInventoryViewModel;
class UASItemInstance;
class UTexture2D;

/** A snapshot for both native widgets and UMG MVVM bindings. No live item reads in the view. */
USTRUCT(BlueprintType)
struct FASItemSlotPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGuid ItemId;
	UPROPERTY(BlueprintReadOnly) FText Name;
	UPROPERTY(BlueprintReadOnly) FText Quantity;
	UPROPERTY(BlueprintReadOnly) FText Key;
	UPROPERTY(BlueprintReadOnly, meta=(ScriptName="equipped_label")) FText Equipped;
	UPROPERTY(BlueprintReadOnly) FText Tooltip;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UTexture2D> Icon = nullptr;
	UPROPERTY(BlueprintReadOnly) EASLootRarity Rarity = EASLootRarity::Uncommon;
	UPROPERTY(BlueprintReadOnly) EASItemType ItemType = EASItemType::Consumable;
	UPROPERTY(BlueprintReadOnly) bool bHasItem = false;
	UPROPERTY(BlueprintReadOnly) bool bEquipped = false;
	UPROPERTY(BlueprintReadOnly) bool bCanDrag = false;
	bool operator==(const FASItemSlotPresentation& Other) const;
};

UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASItemSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	void Initialize(UASInventoryViewModel* InInventory, int32 InSlotIndex);
	/** Compatibility for standalone, authored loot entries. Inventory views use Initialize instead. */
	void SetStandaloneItem(UASItemInstance* Item);
	void Refresh();
	void Release();
	UFUNCTION(BlueprintPure, Category="Inventory|ViewModel") int32 GetSlotIndex() const { return SlotIndex; }
	UFUNCTION(BlueprintPure, Category="Inventory|ViewModel") UASInventoryViewModel* GetInventoryViewModel() const { return Inventory.Get(); }
	UASItemInstance* ResolveItem() const;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Inventory|ViewModel")
	FASItemSlotPresentation Presentation;
private:
	TWeakObjectPtr<UASInventoryViewModel> Inventory;
	TWeakObjectPtr<UASItemInstance> StandaloneItem;
	int32 SlotIndex = INDEX_NONE;
	FASItemAssetRequest IconRequest;
	FSoftObjectPath RequestedIcon;
};
