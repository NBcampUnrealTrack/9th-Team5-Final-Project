#include "GAS/Characters/Enemies/Abilities/ASMonsterProjectileAbility.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"

UASMonsterProjectileAbility::UASMonsterProjectileAbility()
{
	// 발사 시점의 GetAimDirection 결과가 투사체에 전달되어 이후에는 직선으로 진행한다.
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = true;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack);
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_Ranged);
	SetAssetTags(AssetTagsToSet);
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_Attacking);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_Dead);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_HitReact);
}

FVector UASMonsterProjectileAbility::GetAimDirection_Implementation() const
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
