#pragma once

#include "CoreMinimal.h"
#include "Quests/Data/ASQuestTypes.h"
#include "ASQuestSaveData.generated.h"

/** Stable IDs and values only: no runtime actors or asset pointers in a profile. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASQuestSaveRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest|Save")
	FName QuestId;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest|Save")
	EASQuestStatus Status = EASQuestStatus::InProgress;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest|Save")
	TArray<FASQuestObjectiveProgress> Objectives;
};

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASQuestSaveData
{
	GENERATED_BODY()

	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest|Save")
	int32 DataVersion = CurrentVersion;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest|Save")
	TArray<FASQuestSaveRecord> Quests;

	/** Validate before disk writes and before importing a cooperative client's local profile. */
	bool Validate(FString& OutError) const;
};
