#include "Items/Loot/ASLootDaySpawnSubsystem.h"

#include "Core/Framework/ASGameState.h"
#include "Engine/World.h"
#include "Items/Loot/ASLootSpawnSubsystem.h"
#include "TimerManager.h"

bool UASLootDaySpawnSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

void UASLootDaySpawnSubsystem::Deinitialize()
{
	HandleDayEnded();
	Super::Deinitialize();
}

void UASLootDaySpawnSubsystem::HandleDayStarted(const int32 Day, UDataTable* ScheduleTable, const float DayDuration)
{
	HandleDayEnded();
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || Day < 1
		|| !IsValid(ScheduleTable) || ScheduleTable->GetRowStruct() != FASLootDaySpawnScheduleRow::StaticStruct())
	{
		return;
	}
	if (!FMath::IsFinite(DayDuration) || DayDuration <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootDaySpawn] Day %d has invalid duration %.2f."), Day, DayDuration);
		return;
	}

	TArray<FName> RowNames = ScheduleTable->GetRowNames();
	RowNames.Sort(FNameLexicalLess());
	for (const FName RowName : RowNames)
	{
		const FASLootDaySpawnScheduleRow* Row = ScheduleTable->FindRow<FASLootDaySpawnScheduleRow>(
			RowName, TEXT("LootDaySpawnSchedule"), false);
		if (!Row || Row->DayNumber != Day) continue;

		FString Error;
		if (!Row->Validate(Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[LootDaySpawn] Schedule '%s' is invalid: %s"), *RowName.ToString(), *Error);
			continue;
		}
		if (Row->SecondsAfterDayStart >= DayDuration)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LootDaySpawn] Schedule '%s' is after day %d ends (%.2f / %.2f seconds)."),
				*RowName.ToString(), Day, Row->SecondsAfterDayStart, DayDuration);
			continue;
		}

		const FASLootDaySpawnScheduleRow RowCopy = *Row;
		const float Delay = Row->SecondsAfterDayStart;
		if (Delay <= 0.0f)
		{
			ExecuteScheduleRow(RowName, Day, RowCopy);
			continue;
		}

		FTimerHandle& Handle = ScheduledSpawnTimers.AddDefaulted_GetRef();
		GetWorld()->GetTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateWeakLambda(this, [this, RowName, Day, RowCopy]()
			{
				ExecuteScheduleRow(RowName, Day, RowCopy);
			}), Delay, false);
	}
}

void UASLootDaySpawnSubsystem::HandleDayEnded()
{
	if (GetWorld())
	{
		for (FTimerHandle& Handle : ScheduledSpawnTimers)
		{
			GetWorld()->GetTimerManager().ClearTimer(Handle);
		}
	}
	ScheduledSpawnTimers.Reset();
}

void UASLootDaySpawnSubsystem::ExecuteScheduleRow(const FName RowName, const int32 Day,
	const FASLootDaySpawnScheduleRow Row)
{
	UWorld* World = GetWorld();
	const AASGameState* GameState = World ? World->GetGameState<AASGameState>() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || !GameState
		|| GameState->GetCurrentPhase() != EGamePhase::Day || GameState->GetCurrentDay() != Day)
	{
		return;
	}

	UASLootSpawnSubsystem* SpawnSubsystem = World->GetSubsystem<UASLootSpawnSubsystem>();
	if (!SpawnSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootDaySpawn] Schedule '%s' has no loot spawn subsystem."), *RowName.ToString());
		return;
	}
	const int32 Seed = Row.bUseRandomSeed ? FMath::Rand() : Row.Seed;
	FASLootSpawnBatchResult Result;
	if (Row.Placement == EASLootDaySpawnPlacement::RandomArea)
	{
		Result = SpawnSubsystem->SpawnChestsInBox(Row.LootPreset, Row.RandomBoundsTransform,
			Row.RandomBoundsHalfExtent, Row.RandomSpawnSettings, Seed);
	}
	else
	{
		Result = SpawnSubsystem->SpawnChestsAtTransforms(Row.LootPreset, Row.FixedLocations, Seed,
			Row.RandomSpawnSettings.MinSpacing);
	}

	if (Result.Status != EASLootSpawnBatchStatus::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootDaySpawn] Schedule '%s' on day %d spawned %d/%d chests: %s"),
			*RowName.ToString(), Day, Result.Chests.Num(), Result.RequestedCount, *Result.ErrorMessage);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[LootDaySpawn] Schedule '%s' spawned %d chest(s) on day %d."),
			*RowName.ToString(), Result.Chests.Num(), Day);
	}
}
