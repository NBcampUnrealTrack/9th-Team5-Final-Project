#include "GAS/Characters/Enemies/Abilities/ASMonsterMeleeAbility.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"

UASMonsterMeleeAbility::UASMonsterMeleeAbility()
{
	// UASSkillMelee의 범위 판정/피해 적용을 재사용하고 몬스터 태그만 명확히 부여한다.
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = true;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	AbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
	AbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack_Melee);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_Dead);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_HitReact);
}

FVector UASMonsterMeleeAbility::GetAimDirection_Implementation() const
{
	const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetAvatarActorFromActorInfo());
	const UASMonsterTargetingComponent* Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* Target = Targeting ? Targeting->GetTargetActor() : nullptr;
	if (Monster && IsValid(Target))
	{
		const FVector ToTarget = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			return ToTarget;
		}
	}

	return Super::GetAimDirection_Implementation();
}
