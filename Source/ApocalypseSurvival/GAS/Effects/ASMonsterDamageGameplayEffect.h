#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/ASDamageGameplayEffect.h"
#include "ASMonsterDamageGameplayEffect.generated.h"

/** Actor 기반 몬스터 Ability가 SetByCaller(Data.Damage)로 적용하는 즉발 체력 피해 효과. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMonsterDamageGameplayEffect : public UASDamageGameplayEffect
{
	GENERATED_BODY()

public:
	UASMonsterDamageGameplayEffect();
};
