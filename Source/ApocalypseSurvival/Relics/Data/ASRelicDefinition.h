#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ASRelicDefinition.generated.h"

class UASAbilitySet;
class UTexture2D;

/** Static design data. Runtime ownership and effect handles live in UASRelicComponent. */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASRelicDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable identifier used for duplicate checks. Do not derive gameplay logic from the asset name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic")
	FName RelicId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic|Visual")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Descriptive build links for reward UI and future compatibility rules. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic|Build", meta = (Categories = "Build"))
	FGameplayTagContainer SynergyTags;

	/** Existing project ability-set format: grants this relic's GameplayEffects and/or GameplayAbilities. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic|GAS")
	TObjectPtr<const UASAbilitySet> AbilitySet;
};
