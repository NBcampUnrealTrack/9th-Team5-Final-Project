#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Quests/Data/ASQuestTypes.h"
#include "ASQuestDefinition.generated.h"

/** One quest's immutable content. All objectives run in parallel and must be completed. */
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

	/** Also checked at runtime: editor metadata alone cannot reject malformed assets. */
	bool ValidateDefinition(FString& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
