#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ASItemGameplayAbility.generated.h"

class UASItemDefinition;
class UASItemInstance;

/** Shared source-object access for weapon and consumable abilities granted by inventory. */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIAL_API UASItemGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASItemGameplayAbility();

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	UASItemInstance* GetSourceItem() const;

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	UASItemDefinition* GetSourceItemDefinition() const;

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	float GetItemBaseDamage() const;

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	float GetItemHealAmount() const;
};
