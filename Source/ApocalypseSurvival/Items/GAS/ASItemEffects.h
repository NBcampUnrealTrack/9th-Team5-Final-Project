#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ASItemEffects.generated.h"

/** Instant Health addition; Data.Item.Heal is a positive SetByCaller value. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASItemHealingEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UASItemHealingEffect();
};

/** Instant Health addition; uses the existing negative Data.Damage contract. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASItemDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UASItemDamageEffect();
};
