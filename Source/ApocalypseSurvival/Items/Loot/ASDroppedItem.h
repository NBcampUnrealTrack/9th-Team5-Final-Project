#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/ASInteractable.h"
#include "ASDroppedItem.generated.h"

class AASDroppedItem;
class UASInventoryComponent;
class UASItemInstance;
class UBoxComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UTextBlock;

/** Screen-facing item name; right-clicking the name uses the same pickup request as the mesh. */
UCLASS()
class APOCALYPSESURVIVAL_API UASDroppedItemNameWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetItem(AASDroppedItem* InItem);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AASDroppedItem> ItemActor;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
};

/** One world item stack. Right-click picks it up directly, without opening a loot window. */
UCLASS()
class APOCALYPSESURVIVAL_API AASDroppedItem : public AActor, public IASInteractable
{
	GENERATED_BODY()
public:
	AASDroppedItem();
	UASInventoryComponent* GetLootInventory() const { return DroppedInventory; }
	UASItemInstance* GetDroppedItem() const;
	int32 Pickup(APawn* Interactor, UASInventoryComponent* TargetInventory);

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FVector GetInteractionLocation_Implementation() const override { return GetActorLocation(); }
	virtual float GetInteractionRange_Implementation() const override { return 250.0f; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UFUNCTION()
	void HandleInventoryChanged();
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> InteractionCollision;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ItemMesh;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> NameLabel;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UASInventoryComponent> DroppedInventory;
};