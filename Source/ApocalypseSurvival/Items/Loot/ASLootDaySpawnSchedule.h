#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Items/Loot/ASLootSpawnSubsystem.h"
#include "ASLootDaySpawnSchedule.generated.h"

UENUM(BlueprintType)
enum class EASLootDaySpawnPlacement : uint8
{
	RandomArea,
	FixedLocations
};

/** One timed chest-spawn event for a particular day. Time is elapsed gameplay seconds after day start. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASLootDaySpawnScheduleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schedule", meta = (ClampMin = "1"))
	int32 DayNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schedule", meta = (ClampMin = "0.0", Units = "s"))
	float SecondsAfterDayStart = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schedule")
	EASLootDaySpawnPlacement Placement = EASLootDaySpawnPlacement::RandomArea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot",
		meta = (RowType = "/Script/ApocalypseSurvival.ASLootChestPresetRow"))
	FDataTableRowHandle LootPreset;

	/** Used for a random-area spawn. Half extents are in local, unscaled coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement|Random",
		meta = (EditCondition = "Placement == EASLootDaySpawnPlacement::RandomArea", EditConditionHides))
	FTransform RandomBoundsTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement|Random",
		meta = (EditCondition = "Placement == EASLootDaySpawnPlacement::RandomArea", EditConditionHides,
		ClampMin = "1.0", Units = "cm"))
	FVector RandomBoundsHalfExtent = FVector(1000.0, 1000.0, 500.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement|Fixed",
		meta = (EditCondition = "Placement == EASLootDaySpawnPlacement::FixedLocations", EditConditionHides))
	TArray<FTransform> FixedLocations;

	/** Random-area count, spacing, ground projection, and rotation options. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement|Options")
	FASLootRandomSpawnSettings RandomSpawnSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Random")
	bool bUseRandomSeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Random", meta = (EditCondition = "!bUseRandomSeed"))
	int32 Seed = 12345;

	bool Validate(FString& OutError) const;
};
