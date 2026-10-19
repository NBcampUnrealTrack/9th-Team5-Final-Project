#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ASModulePassiveAbility.generated.h"

/** ModuleComponent starts these on the server and restarts them for a new avatar after respawn.
 * Derived abilities implement their condition/effect and release delegates/tasks in EndAbility.
 * Ordinary event-triggered GAs can also be granted through the module AbilitySet.
 */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASModulePassiveAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASModulePassiveAbility();

	/** Numeric core value captured when this module was applied for the current run. */
	UFUNCTION(BlueprintPure, Category = "Modules")
	bool GetModuleCoreValue(float& OutValue) const;
};
