#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ASQuestNpcDefinition.generated.h"

/** Identity and dialogue shared by instances of an NPC across lobby and game maps. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASQuestNpcDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC")
	FName NpcId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC")
	FText DisplayName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC", meta = (MultiLine = true))
	FText IdleDialogue;
	/** Optional target for Quest.Event.Interacted objectives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC", meta = (Categories = "Quest.Target"))
	FGameplayTag InteractionTargetTag;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
