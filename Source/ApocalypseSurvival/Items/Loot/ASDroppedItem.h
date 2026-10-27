#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Interaction/ASInteractable.h"
#include "Items/Data/ASItemAssetLoading.h"
#include "ASDroppedItem.generated.h"

class AASDroppedItem;
class UASInventoryComponent;
class UASItemInstance;
class UBoxComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UTextBlock;
class UBorder;
class UASItemInteractionComponent;

/** Moves a hovered nameplate above other world labels while preserving the same widget and input. */
UCLASS()
class APOCALYPSESURVIVAL_API UASDroppedItemNameComponent : public UWidgetComponent
{
	GENERATED_BODY()
public:
	void SetRaised(bool bRaised);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	// Keep the Slate widget alive until the component joins its new layer on the next tick.
	TSharedPtr<SWidget> TransitionWidget;
};

/** Screen-facing item name; the only mouse pickup target for a dropped item. */
UCLASS()
class APOCALYPSESURVIVAL_API UASDroppedItemNameWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetItem(AASDroppedItem* InItem);
	void RefreshVisibility();
	void SetWorldHovered(bool bHovered);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& Event) override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
	void RefreshHoverAppearance();
	UASItemInteractionComponent* GetLocalInteraction() const;
	UPROPERTY(Transient)
	TWeakObjectPtr<AASDroppedItem> ItemActor;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> NameBackground;
	bool bPointerHovered = false;
	bool bWorldHovered = false;
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
	/** Local cosmetic state only; each player's item interaction component updates it. */
	void SetWorldHovered(bool bHovered);
	void RefreshNameVisibility();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FVector GetInteractionLocation_Implementation() const override { return GetActorLocation(); }
	virtual float GetInteractionRange_Implementation() const override { return 250.0f; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	/** Server placement shared by inventory, chest and monster drops, including late mesh loads. */
	void SeparateFromNearbyDrops();
	float GetDropFootprintRadius() const;
	/** Minimum centre-to-centre distance. Larger meshes also reserve their actual footprint. */
	UPROPERTY(EditDefaultsOnly, Category = "Item|Drop", meta = (ClampMin = "30.0", Units = "cm"))
	float MinimumDropSpacing = 100.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Item|Drop", meta = (ClampMin = "150.0", Units = "cm"))
	float DropSearchRadius = 1500.0f;
	UFUNCTION()
	void HandleInventoryChanged();
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> InteractionCollision;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ItemMesh;
	/** Cosmetic only; enabled on clients while a Legendary item remains in this drop. */
	UPROPERTY(VisibleAnywhere, Category = "Item|Appearance")
	TObjectPtr<UStaticMeshComponent> LegendaryBeam;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> NameLabel;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UASInventoryComponent> DroppedInventory;
	FASItemAssetRequest MeshAssetRequest;
	bool bWorldHovered = false;
};
