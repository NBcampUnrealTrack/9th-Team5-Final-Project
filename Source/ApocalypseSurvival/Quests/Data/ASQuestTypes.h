#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/Data/ASItemGrant.h"
#include "ASQuestTypes.generated.h"

class UASQuestDefinition;
class UASItemDefinition;
class UASMonsterDefinition;
class APlayerState;
class AActor;

UENUM(BlueprintType)
enum class EASQuestObjectiveType : uint8
{
	Custom UMETA(DisplayName = "Custom Event (Advanced)"),
	ItemCollected UMETA(DisplayName = "Collect Item"),
	ZoneEntered UMETA(DisplayName = "Enter Area"),
	EnemyKilled UMETA(DisplayName = "Defeat Monster"),
	Interacted UMETA(DisplayName = "Interact with NPC")
};

UENUM(BlueprintType)
enum class EASQuestKillCredit : uint8
{
	SharedRun,
	KillerOnly
};

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

	/** Identity from the actual defeated monster; no quest-specific monster variant is required. */
	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UASMonsterDefinition> MonsterDefinition;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TObjectPtr<APlayerState> KillerPlayerState;
};

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASQuestObjectiveDefinition
{
	GENERATED_BODY()

	/** Existing event-tag assets are upgraded on load without changing objective/save IDs. */
	void UpgradeLegacyType();
	FGameplayTag GetEventTag() const;
	FGameplayTag GetTargetTag() const;
	UASItemDefinition* GetItemDefinition() const;
	UASMonsterDefinition* GetTargetMonsterDefinition() const;
	EASQuestKillCredit GetKillCredit() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	EASQuestObjectiveType ObjectiveType = EASQuestObjectiveType::Custom;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FName ObjectiveId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (Categories = "Quest.Event", EditCondition = "ObjectiveType == EASQuestObjectiveType::Custom", EditConditionHides))
	FGameplayTag EventTag;

	/** Empty matches any subject. Otherwise a matching child tag also counts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (Categories = "Quest.Target", EditCondition = "ObjectiveType == EASQuestObjectiveType::ZoneEntered || ObjectiveType == EASQuestObjectiveType::Interacted || ObjectiveType == EASQuestObjectiveType::Custom", EditConditionHides))
	FGameplayTag TargetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	/** Item-collected objectives with this field can only advance through server loot routing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (EditCondition = "ObjectiveType == EASQuestObjectiveType::ItemCollected || ObjectiveType == EASQuestObjectiveType::Custom", EditConditionHides))
	TObjectPtr<UASItemDefinition> ItemDefinition;

	/** Existing monster data asset to defeat. Empty counts any monster. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Monster", meta = (EditCondition = "ObjectiveType == EASQuestObjectiveType::EnemyKilled || ObjectiveType == EASQuestObjectiveType::Custom", EditConditionHides))
	TObjectPtr<UASMonsterDefinition> TargetMonsterDefinition;

	/** SharedRun grants personal progress to eligible players in this run; KillerOnly requires the final blow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Monster", meta = (EditCondition = "ObjectiveType == EASQuestObjectiveType::EnemyKilled || ObjectiveType == EASQuestObjectiveType::Custom", EditConditionHides))
	EASQuestKillCredit KillCredit = EASQuestKillCredit::SharedRun;

	/** Granted directly to the personal quest bag when this objective reaches RequiredCount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Items")
	TArray<FASItemGrant> CompletionItems;
};

/** Separate personal quest bag, backed by persistent objective receipts rather than normal inventory slots. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASQuestItemEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName QuestId;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UASItemDefinition> ItemDefinition;
	UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
	UPROPERTY(BlueprintReadOnly) int32 RequiredCount = 1;
	UPROPERTY(BlueprintReadOnly) EASQuestStatus Status = EASQuestStatus::InProgress;
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

	/** Shared by server progression, personal loot eligibility, save restore and UI. */
	bool HasValidObjectives() const;
	bool IsObjectiveComplete(int32 Index) const;
	bool AreObjectivesComplete() const;
	/** Snapshot before applying an event: a sequential quest exposes at most one objective. */
	TArray<int32> GetEligibleObjectiveIndices() const;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UASQuestDefinition> Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	EASQuestStatus Status = EASQuestStatus::InProgress;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TArray<FASQuestObjectiveProgress> Objectives;
};

namespace ASQuestRules
{
	/** Uses the same item identity/tag rules for loot eligibility and actual event application. */
	APOCALYPSESURVIVAL_API bool MatchesObjective(const FASQuestObjectiveDefinition& Objective,
		const FASQuestEvent& Event, const UASItemDefinition* CollectedItem, const AActor* QuestOwner);
}
