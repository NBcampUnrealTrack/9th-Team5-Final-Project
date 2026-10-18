#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/ASDamageGameplayEffect.h"
#include "ASMassMonsterDamageEffect.generated.h"

/** Mass 몬스터가 ASC 대상에게 적용하는 즉발 Health 피해 효과. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterDamageEffect : public UASDamageGameplayEffect
{
	GENERATED_BODY()

public:
	UASMassMonsterDamageEffect();
};
