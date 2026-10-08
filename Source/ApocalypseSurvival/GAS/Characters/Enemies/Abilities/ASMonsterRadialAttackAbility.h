#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterMeleeAbility.h"
#include "ASMonsterRadialAttackAbility.generated.h"

/** 몬스터 중심에서 한 번 피해를 주는 360도 근접 범위 공격. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterRadialAttackAbility : public UASMonsterMeleeAbility
{
	GENERATED_BODY()

public:
	UASMonsterRadialAttackAbility();
};
