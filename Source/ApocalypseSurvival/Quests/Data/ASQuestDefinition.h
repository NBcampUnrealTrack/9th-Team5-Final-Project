#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Quests/Data/ASQuestTypes.h"
#include "Items/Data/ASItemGrant.h"
#include "ASQuestDefinition.generated.h"

/** One quest's immutable content. */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Unique across all quest assets used in a session. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (TitleProperty = "ObjectiveId"))
	TArray<FASQuestObjectiveDefinition> Objectives;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Reward", meta = (ClampMin = "0"))
	int32 ExperienceReward = 0;

	/** Claimed quests required before this quest can be accepted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Story")
	TArray<FName> PrerequisiteQuestIds;

	/** Bootstrap only: quests with prerequisites always require explicit acceptance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Start")
	bool bAutoAccept = false;

	/** Empty uses TurnInNpcId, preserving existing quests. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Start")
	FName OfferNpcId;

	/** Higher priorities appear first within available offers, reminders, or hand-ins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Dialogue")
	int32 DialoguePriority = 0;

	FName GetOfferNpcId() const { return OfferNpcId.IsNone() ? TurnInNpcId : OfferNpcId; }

	/** When true, only the first unfinished objective can receive an event. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Story")
	bool bSequentialObjectives = false;

	/** In-progress evidence belongs to this run. Only a completed boss clear is persisted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Story")
	bool bRunBoundObjectives = false;

	/** Non-empty quests can only be handed in through this NPC's dialogue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Story")
	FName TurnInNpcId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Dialogue", meta = (MultiLine = true))
	TArray<FText> OfferDialogue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Dialogue", meta = (MultiLine = true))
	TArray<FText> ProgressDialogue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Dialogue", meta = (MultiLine = true))
	TArray<FText> CompletionDialogue;

	/** Quest items held after all objectives are complete. Kept under the old name for asset compatibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Items", meta = (DisplayName = "Completion Items"))
	TArray<FASItemGrant> BossClearItems;

	/** Also checked at runtime: editor metadata alone cannot reject malformed assets. */
	bool ValidateDefinition(FString& OutError) const;
	virtual void PostLoad() override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	/** Zero identifies assets authored before objective-type editing was introduced. */
	UPROPERTY()
	int32 ObjectiveDataVersion = 0;
};
