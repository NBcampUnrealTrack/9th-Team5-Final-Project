#include "GAS/Characters/Enemies/Abilities/ASMonsterTargetedAttackAbility.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"

UASMonsterTargetedAttackAbility::UASMonsterTargetedAttackAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = true;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	AbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack_Targeted);
	AbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_Attacking);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_Dead);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_HitReact);
}

void UASMonsterTargetedAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	LockedTarget.Reset();

	const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetAvatarActorFromActorInfo());
	const UASMonsterTargetingComponent* Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	if (AActor* Target = Targeting ? Targeting->GetTargetActor() : nullptr; IsValid(Target))
	{
		LockedTarget = Target;
	}

	// 타깃이 없는 경우에는 Commit하지 않는다. CombatComponent가 다음 후보를 선택할 수 있다.
	if (!LockedTarget.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UASMonsterTargetedAttackAbility::OnSkillEffect_Implementation()
{
	AActor* Target = LockedTarget.Get();
	if (HasSkillAuthority() && IsValidSkillTarget(Target, GetAvatarActorFromActorInfo(), bCanHitPlayers))
	{
		ApplySkillDamage(Target);
	}
}

FVector UASMonsterTargetedAttackAbility::GetAimDirection_Implementation() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const AActor* Target = LockedTarget.Get();
	if (IsValid(Avatar) && IsValid(Target))
	{
		const FVector ToTarget = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			return ToTarget;
		}
	}

	return Super::GetAimDirection_Implementation();
}
