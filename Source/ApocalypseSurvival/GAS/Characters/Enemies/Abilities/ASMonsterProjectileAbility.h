#pragma once

#include "CoreMinimal.h"
#include "Skills/Projectile/ASSkillProjectile.h"
#include "ASMonsterProjectileAbility.generated.h"

/** 몬스터 직선 투사체 기본 공격 Blueprint의 부모 클래스. */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterProjectileAbility : public UASSkillProjectile
{
	GENERATED_BODY()

public:
	UASMonsterProjectileAbility();

protected:
	virtual FVector GetAimDirection_Implementation() const override;
};
