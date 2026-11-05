#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ASQuestCatalog.generated.h"

class UASQuestDefinition;

/** Author the available quests here. Array order breaks ties between equal dialogue priorities. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASQuestCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quests")
	TArray<TObjectPtr<UASQuestDefinition>> Quests;

	static bool ValidateDefinitions(const TArray<TObjectPtr<UASQuestDefinition>>& Definitions, FString& OutError);
	bool ValidateCatalog(FString& OutError) const { return ValidateDefinitions(Quests, OutError); }
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
