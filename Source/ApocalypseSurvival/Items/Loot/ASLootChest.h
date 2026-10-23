#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ASInteractable.h"
#include "Items/Data/ASItemGrant.h"
#include "Items/Loot/ASLootDefinition.h"
#include "ASLootChest.generated.h"

class AASLootChest;
class UASInventoryComponent;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FASLootInteractionRequestedSignature,
	AASLootChest*, LootChest,
	AActor*, Interactor);

/** A world container whose contents can be transferred into another inventory. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASLootChest : public AActor, public IASInteractable
{
	GENERATED_BODY()

public:
	AASLootChest();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Loot")
	UASInventoryComponent* GetLootInventory() const { return LootInventory; }

	/** Replace default contents before FinishSpawning/BeginPlay. Empty explicitly spawns an empty chest.
	 * Server-only. Returns false for invalid grants or after initialization has begun.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	bool SetInitialLoot(const TArray<FASItemGrant>& Items);

	/** Applies preset rarity appearance on the server; the resulting visual settings replicate to clients. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot|Appearance")
	void SetLootRarityAppearance(EASLootRarity Rarity, UStaticMesh* MeshOverride,
		UMaterialInterface* MaterialOverride, int32 MaterialIndex, FLinearColor ColorParameterValue, FName ColorParameter);

	UFUNCTION(BlueprintPure, Category = "Loot|Appearance")
	EASLootRarity GetLootRarity() const { return LootRarity; }

	/** Applied once after construction. Also used by BeginPlay; returns the cached result on repeat calls.
	 * Failed additions are rolled back. Spawn callers destroy the rejected chest.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	bool InitializeInitialLoot(FString& OutError);
	const FString& GetInitialLootError() const { return InitialLootError; }

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Blueprint hook for opening a loot widget or playing presentation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot", meta = (DisplayName = "On Loot Interaction Requested"))
	void BP_OnLootInteractionRequested(AActor* Interactor);

private:
	bool IsWithinInteractionRange(const AActor* Interactor) const;
	void ApplyLootRarityAppearance();
	void UpdateEmptyDespawnTimer();
	void DespawnIfStillEmpty();

	UFUNCTION()
	void HandleLootInventoryChanged();

	UFUNCTION()
	void OnRep_LootRarityAppearance();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ChestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASInventoryComponent> LootInventory;

	/** Spawn input overrides the Blueprint defaults; applied once on the server before Blueprint BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (AllowPrivateAccess = "true", ExposeOnSpawn = "true"))
	TArray<FASItemGrant> InitialLoot;

	/** Preserve explicit API input if a Blueprint construction script changes InitialLoot defaults. */
	UPROPERTY(Transient)
	TArray<FASItemGrant> SpawnInitialLoot;
	bool bHasSpawnInitialLoot = false;
	bool bInitialLootAttempted = false;
	bool bInitialLootSucceeded = false;
	FString InitialLootError;

	UPROPERTY(ReplicatedUsing = OnRep_LootRarityAppearance, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Loot|Appearance", meta = (AllowPrivateAccess = "true"))
	EASLootRarity LootRarity = EASLootRarity::Uncommon;
	UPROPERTY(ReplicatedUsing = OnRep_LootRarityAppearance, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Loot|Appearance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> LootMeshOverride;
	UPROPERTY(ReplicatedUsing = OnRep_LootRarityAppearance, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Loot|Appearance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> LootMaterialOverride;
	UPROPERTY(ReplicatedUsing = OnRep_LootRarityAppearance, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Loot|Appearance", meta = (AllowPrivateAccess = "true"))
	int32 LootAppearanceMaterialIndex = 0;
	UPROPERTY(ReplicatedUsing = OnRep_LootRarityAppearance, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Loot|Appearance", meta = (AllowPrivateAccess = "true"))
	FLinearColor LootRarityColor = FLinearColor::White;
	UPROPERTY(ReplicatedUsing = OnRep_LootRarityAppearance, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Loot|Appearance", meta = (AllowPrivateAccess = "true"))
	FName LootRarityColorParameter = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float InteractionRange = 250.0f;

	/** Seconds an empty chest remains in the world. Set to 0 to remove it immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float EmptyDespawnDelay = 30.0f;

	FTimerHandle EmptyDespawnTimerHandle;
};
