#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ASAbilitySystemComponent.generated.h"

/**
 * Project ability system component.
 *
 * Players own theirs on AASPlayerState so attributes and granted abilities
 * survive a respawn. Kept as its own class so project-wide ability rules
 * (input slot routing, ability sets) have one place to live.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIAL_API UASAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UASAbilitySystemComponent();
};
