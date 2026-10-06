#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ASInteractable.h"
#include "Items/Data/ASItemGrant.h"
#include "ASLootChest.generated.h"

class AASLootChest;
class UASInventoryComponent;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FASLootInteractionRequestedSignature,
	AASLootChest*, LootChest,
	AActor*, Interactor);

/** A world container whose contents can be transferred into another inventory. */
UCLASS(Blueprintable)
class APOCALYPSESURVIAL_API AASLootChest : public AActor, public IASInteractable
{
	GENERATED_BODY()

public:
	AASLootChest();

	UFUNCTION(BlueprintPure, Category = "Loot")
	UASInventoryComponent* GetLootInventory() const { return LootInventory; }

	/** Server revalidates range, recipient ownership and remaining quantity for every pickup. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	int32 TakeLoot(APawn* Interactor, UASInventoryComponent* TargetInventory, FGuid InstanceId, int32 Quantity);

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FVector GetInteractionLocation_Implementation() const override;
	virtual float GetInteractionRange_Implementation() const override;

	/** UI/controller code can subscribe without the chest depending on a widget class. */
	UPROPERTY(BlueprintAssignable, Category = "Loot")
	FASLootInteractionRequestedSignature OnLootInteractionRequested;

protected:
	virtual void BeginPlay() override;

	/** Blueprint hook for opening a loot widget or playing presentation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot", meta = (DisplayName = "On Loot Interaction Requested"))
	void BP_OnLootInteractionRequested(AActor* Interactor);

private:
	bool IsWithinInteractionRange(const AActor* Interactor) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ChestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASInventoryComponent> LootInventory;

	/** Items generated into LootInventory when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true"))
	TArray<FASItemGrant> InitialLoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float InteractionRange = 250.0f;
};
