#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ASModuleDefinition.generated.h"

class UASAbilitySet;
class UTexture2D;
class FDataValidationContext;

/** Shared module data. Ownership and lobby selections are stored separately by ModuleId. */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASModuleDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable save identifier; independent of the asset's name and location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module")
	FName ModuleId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Infinite, non-periodic effects and granted abilities. Module passives activate automatically. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module|GAS")
	TObjectPtr<UASAbilitySet> AbilitySet;

	bool ValidateDefinition(FString& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
