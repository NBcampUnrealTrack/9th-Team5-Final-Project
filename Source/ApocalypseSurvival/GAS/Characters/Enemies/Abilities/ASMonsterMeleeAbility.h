#pragma once

#include "CoreMinimal.h"
#include "Skills/Melee/ASSkillMelee.h"
#include "ASMonsterMeleeAbility.generated.h"

/** 몬스터 근거리 기본 공격 Blueprint의 부모 클래스. */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterMeleeAbility : public UASSkillMelee
{
	GENERATED_BODY()

public:
	UASMonsterMeleeAbility();

protected:
	virtual FVector GetAimDirection_Implementation() const override;
};
