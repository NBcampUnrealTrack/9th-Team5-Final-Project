#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ASQuestTypes.generated.h"

class UASQuestDefinition;

namespace ASQuestLimits
{
	inline constexpr int32 MaxQuests = 256;
	inline constexpr int32 MaxObjectives = 64;
}

UENUM(BlueprintType)
enum class EASQuestStatus : uint8
{
	InProgress,
	ReadyToTurnIn,
	RewardClaimed
};

/** Events are reported by trusted server gameplay code, never by the client UI. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASQuestEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (Categories = "Quest.Event"))
	FGameplayTag EventTag;

	/** Optional subject, such as an enemy type, item type, or zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FGameplayTag TargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "1"))
	int32 Amount = 1;
};

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASQuestObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FName ObjectiveId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (Categories = "Quest.Event"))
	FGameplayTag EventTag;

	/** Empty matches any subject. Otherwise a matching child tag also counts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FGameplayTag TargetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;
};

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASQuestObjectiveProgress
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest")
	FName ObjectiveId;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest")
	int32 CurrentCount = 0;
};

/** Per-player runtime data. Shared definition assets are never mutated. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UASQuestDefinition> Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	EASQuestStatus Status = EASQuestStatus::InProgress;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TArray<FASQuestObjectiveProgress> Objectives;
};
