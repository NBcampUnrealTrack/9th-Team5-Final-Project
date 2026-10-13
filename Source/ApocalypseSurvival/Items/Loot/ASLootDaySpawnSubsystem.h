#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Items/Loot/ASLootDaySpawnSchedule.h"
#include "ASLootDaySpawnSubsystem.generated.h"

class UDataTable;

/** Runs the day's chest schedule on the authoritative world and delegates placement to the loot spawn subsystem. */
UCLASS()
class APOCALYPSESURVIVAL_API UASLootDaySpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	void HandleDayStarted(int32 Day, UDataTable* ScheduleTable, float DayDuration);
	void HandleDayEnded();

private:
	void ExecuteScheduleRow(FName RowName, int32 Day, FASLootDaySpawnScheduleRow Row);

	TArray<FTimerHandle> ScheduledSpawnTimers;
};
