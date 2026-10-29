#pragma once

#include "GAS/Abilities/ASGameplayAbility_CharacterPrimaryAttack.h"
#include "ASGameplayAbility_CharacterDirectionalAttack.generated.h"

/**
 * 이동·자동 타깃 탐색 없이 클릭 방향으로 일반 공격을 수행한다.
 * 피해, 탄약, 몽타주, 서버 검증은 부모 PrimaryAttack의 공통 흐름을 사용한다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASGameplayAbility_CharacterDirectionalAttack
	: public UASGameplayAbility_CharacterPrimaryAttack
{
	GENERATED_BODY()

public:
	virtual bool IsAttackMoveAbility() const override { return false; }

protected:
	virtual EASPrimaryAttackMode GetAttackMode() const override { return EASPrimaryAttackMode::Directional; }
	virtual FGameplayTag GetTargetingStateTag() const override;
	virtual bool ShouldConsumeAttackMoveRequest() const override { return false; }
};
