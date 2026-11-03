#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ASInventoryViewModel.generated.h"

class UASInventoryComponent;
class UASItemSlotViewModel;
class UASItemInteractionComponent;
class APlayerController;
class AASLootChest;

USTRUCT(BlueprintType)
struct FASInventoryPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 StorageSlotCount = 16;
	UPROPERTY(BlueprintReadOnly) int32 ItemCount = 0;
	UPROPERTY(BlueprintReadOnly) FText Capacity;
	UPROPERTY(BlueprintReadOnly) FText Feedback;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FLinearColor TitleColor = FLinearColor(0.35f, 0.73f, 0.92f);
	bool operator==(const FASInventoryPresentation& Other) const;
};

/** Local presentation/commands only. All changes still use the existing server RPC bridge. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASInventoryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	void Initialize(UASInventoryComponent* Source, APlayerController* Controller);
	void Shutdown();
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category="Inventory|ViewModel") void Refresh();
	UFUNCTION(BlueprintPure, Category="Inventory|ViewModel") UASItemSlotViewModel* GetSlot(int32 Index);
	UASInventoryComponent* GetInventory() const { return Inventory.Get(); }
	AASLootChest* GetLootChest() const;
	UFUNCTION(BlueprintCallable, Category="Inventory|ViewModel") void ActivateSlot(int32 Slot, FGuid Expected);
	UFUNCTION(BlueprintCallable, Category="Inventory|ViewModel") void DropSlot(int32 Slot, FGuid Expected, bool bSingle = false);
	UFUNCTION(BlueprintCallable, Category="Inventory|ViewModel") void MoveSlot(int32 From, int32 To, FGuid ExpectedFrom, FGuid ExpectedTo);
	UFUNCTION(BlueprintCallable, Category="Inventory|ViewModel") void LootItem(FGuid Expected);
	UFUNCTION(BlueprintCallable, Category="Inventory|ViewModel") void DropLootItem(FGuid Expected);
	UFUNCTION(BlueprintCallable, Category="Inventory|ViewModel") void CloseInventory();
	UFUNCTION(BlueprintCallable, Category="Inventory|ViewModel") void OpenCrafting();
	void ClearFeedback();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Inventory|ViewModel") FASInventoryPresentation Presentation;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Inventory|ViewModel") TArray<TObjectPtr<UASItemSlotViewModel>> QuickSlots;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Inventory|ViewModel") TArray<TObjectPtr<UASItemSlotViewModel>> StorageSlots;
	/** Includes empty positions; suitable for a four-column loot grid. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Inventory|ViewModel") TArray<TObjectPtr<UASItemSlotViewModel>> ContainerSlots;
private:
	UFUNCTION() void HandleActionFailed(FText Message);
	bool MatchesSlot(int32 Slot, FGuid Expected) const;
	bool IsPlayerSource() const;
	bool IsExposedSlot(int32 Index) const;
	TWeakObjectPtr<UASInventoryComponent> Inventory;
	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<UASItemInteractionComponent> Interaction;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<UASItemSlotViewModel>> SlotModels;
};
