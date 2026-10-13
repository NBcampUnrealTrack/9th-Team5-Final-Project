#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Items/Loot/ASLootBlueprintLibrary.h"
#include "ASLootSpawnSubsystem.generated.h"

UENUM(BlueprintType)
enum class EASLootSpawnBatchStatus : uint8
{
	Success,
	PartialSuccess,
	InvalidRequest,
	NotAuthority,
	SpawnFailed
};

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASLootSpawnBatchResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	EASLootSpawnBatchStatus Status = EASLootSpawnBatchStatus::SpawnFailed;
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	int32 RequestedCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	int32 AttemptCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	TArray<TObjectPtr<AASLootChest>> Chests;
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	FString ErrorMessage;
};

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASLootRandomSpawnSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 Count = 5;
	/** Total candidate limit, including rejected ground/spacing/collision checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 MaxAttempts = 100;
	/** Horizontal distance from all living chests managed by this subsystem, including previous requests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSpacing = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bRandomYaw = true;
	/** Trace from the box's top to bottom. Disabled: sample throughout the box, without ground checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Ground")
	bool bProjectToGround = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Ground", meta = (EditCondition = "bProjectToGround"))
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Ground", meta = (EditCondition = "bProjectToGround", ClampMin = "0.0", Units = "cm"))
	float GroundOffset = 10.0f;
	/** 1 is flat ground; 0 allows any upward-facing slope. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Ground", meta = (EditCondition = "bProjectToGround", ClampMin = "0.0", ClampMax = "1.0"))
	float MinGroundNormalZ = 0.7f;
};

/** Server world service. Placement and game rules supply presets/coordinates; this owns spawning and tracking. */
UCLASS()
class APOCALYPSESURVIVAL_API UASLootSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot")
	FASLootChestSpawnResult SpawnChestAtTransform(const FDataTableRowHandle& Preset,
		const FTransform& Transform, int32 Seed);

	/** Every supplied transform is tried once. Successful chests remain when another position fails. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot")
	FASLootSpawnBatchResult SpawnChestsAtTransforms(const FDataTableRowHandle& Preset,
		const TArray<FTransform>& Transforms, int32 Seed, float MinSpacing = 0.0f);

	/** Oriented/scaled box, with unscaled half extents. Each call requests additional chests. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot")
	FASLootSpawnBatchResult SpawnChestsInBox(const FDataTableRowHandle& Preset,
		const FTransform& BoxTransform, FVector HalfExtent, const FASLootRandomSpawnSettings& Settings, int32 Seed);

	UFUNCTION(BlueprintPure, Category = "Items|Loot")
	int32 GetManagedChestCount() const;

	/** Explicit cleanup of this service's chests only. World teardown otherwise handles actor destruction. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Items|Loot")
	void DestroyManagedChests();

private:
	bool CanSpawn() const;
	bool HasSpacing(const FVector& Location, float MinSpacing) const;
	FASLootChestSpawnResult SpawnManagedChest(const FDataTableRowHandle& Preset, const FTransform& Transform, int32 Seed);
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AASLootChest>> ManagedChests;
	bool bSpawnInProgress = false;
};
