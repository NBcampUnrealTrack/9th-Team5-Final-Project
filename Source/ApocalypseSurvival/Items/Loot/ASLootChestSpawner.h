#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/Loot/ASLootBlueprintLibrary.h"
#include "ASLootChestSpawner.generated.h"

class USceneComponent;

/** One placed spawn point owns at most one live chest. Spawn timing can also be driven by game code. */
UCLASS()
class APOCALYPSESURVIVAL_API AASLootChestSpawner : public AActor
{
	GENERATED_BODY()

public:
	AASLootChestSpawner();

	/** Creates a chest at this actor's transform. Repeated calls return the existing live chest.
	 * After that chest is destroyed, another call can create a new one. No automatic respawn timer.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot")
	FASLootChestSpawnResult SpawnChest();

	UFUNCTION(BlueprintPure, Category = "Items|Loot")
	AASLootChest* GetSpawnedChest() const;

	/** Select the contents and chest class without hardcoding asset paths. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot",
		meta = (RowType = "/Script/ApocalypseSurvival.ASLootChestPresetRow"))
	FDataTableRowHandle LootPreset;

	/** Disable when another system should decide when to call SpawnChest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bSpawnOnBeginPlay = true;

	/** Randomize this spawn point's stream once on the server. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random")
	bool bUseRandomSeed = true;

	/** With random seeding disabled, identical presets and seeds reproduce the same sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random",
		meta = (EditCondition = "!bUseRandomSeed"))
	int32 Seed = 12345;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TWeakObjectPtr<AASLootChest> SpawnedChest;

	FRandomStream Random;
	bool bRandomInitialized = false;
	bool bSpawnInProgress = false;
};
