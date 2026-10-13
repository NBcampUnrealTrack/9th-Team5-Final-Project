#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/ASDamageGameplayEffect.h"
#include "ASItemEffects.generated.h"

/** Instant Health addition; uses the existing negative Data.Damage contract. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASItemDamageEffect : public UASDamageGameplayEffect
{
	GENERATED_BODY()
public:
	UASItemDamageEffect();
};
