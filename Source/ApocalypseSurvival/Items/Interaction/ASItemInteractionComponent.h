#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASItemInteractionComponent.generated.h"

class AASLootChest;
class AASPlayerController;
class UASInventoryComponent;
class UASLootWidget;

/** Controller-owned RPC bridge. Finds an existing player inventory; never creates one. */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASItemInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASItemInteractionComponent();
	bool TryInteractWith(AActor* Target);
	void CancelPendingInteraction();
	bool HasPendingInteraction() const { return PendingTarget.IsValid(); }
	bool IsLootOpen() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	UASInventoryComponent* GetPlayerInventory() const;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestLoot(AASLootChest* Chest, FGuid InstanceId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestUseItem(FGuid InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestEquipItem(FGuid InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestUnequipItem();

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void CloseLoot();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AASPlayerController* GetASController() const;
	bool CanReachTarget(AActor* Target) const;

	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerTakeLoot(AASLootChest* Chest, FGuid InstanceId, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void ServerUseItem(FGuid InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerEquipItem(FGuid InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerUnequipItem();

	UFUNCTION(Server, Reliable)
	void ServerCloseLoot();

	UFUNCTION(Client, Reliable)
	void ClientOpenLoot(AASLootChest* Chest);

	UFUNCTION()
	void HandleLootClosed();

	UPROPERTY(EditDefaultsOnly, Category = "Loot")
	TSubclassOf<UASLootWidget> LootWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UASLootWidget> LootWidget;

	TWeakObjectPtr<AActor> PendingTarget;
	TWeakObjectPtr<AASLootChest> OpenChest;
	TWeakObjectPtr<AASLootChest> ServerOpenChest;
	TWeakObjectPtr<APawn> ServerLootPawn;
	TWeakObjectPtr<APawn> LootPawn;
	double PendingStartedAt = 0.0;
};
