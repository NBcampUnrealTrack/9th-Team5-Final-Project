#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/Loot/ASLootSpawnSubsystem.h"
#include "ASLootSpawnArea.generated.h"

class UBoxComponent;

/** Map configuration for a random chest batch. The subsystem performs all placement and spawning. */
UCLASS()
class APOCALYPSESURVIVAL_API AASLootSpawnArea : public AActor
{
	GENERATED_BODY()

public:
	AASLootSpawnArea();

	/** Requests another batch. Existing managed chests participate in minimum-spacing checks. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot")
	FASLootSpawnBatchResult SpawnChests();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> SpawnBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot",
		meta = (RowType = "/Script/ApocalypseSurvival.ASLootChestPresetRow"))
	FDataTableRowHandle LootPreset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FASLootRandomSpawnSettings SpawnSettings;

	/** Disable to let game code decide when this area requests a batch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bSpawnOnBeginPlay = true;

	/** Each request uses a fresh random seed; disable for repeatable placement/loot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random")
	bool bUseRandomSeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Random", meta = (EditCondition = "!bUseRandomSeed"))
	int32 Seed = 12345;

protected:
	virtual void BeginPlay() override;
};
