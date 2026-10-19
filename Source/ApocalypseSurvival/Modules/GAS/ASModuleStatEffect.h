#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Modules/Data/ASModuleTypes.h"
#include "ASModuleStatEffect.generated.h"

/** Shared persistent additive equipment effect. No per-module GE assets are required. */
UCLASS()
class APOCALYPSESURVIVAL_API UASModuleStatEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UASModuleStatEffect();
	static void SetMagnitude(FGameplayEffectSpec& Spec, EASModuleStat Stat, float Value);
	static FGameplayTag GetCoreMagnitudeTag();
};
