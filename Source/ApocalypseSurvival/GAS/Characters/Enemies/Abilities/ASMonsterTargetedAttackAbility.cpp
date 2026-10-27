#include "GAS/Characters/Enemies/Abilities/ASMonsterTargetedAttackAbility.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"

UASMonsterTargetedAttackAbility::UASMonsterTargetedAttackAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = true;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	// 0이면 태그 적용과 EndAbility가 같은 프레임에 끝나 경고 색을 볼 수 없다.
	// Blueprint에서 필요하면 개별 패턴의 선딜 시간으로 덮어쓸 수 있다.
	WindupDuration = 0.75f;
	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_Targeted);
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack);
	SetAssetTags(AssetTagsToSet);
	CompletionEventTag = ASGameplayTags::Event_Monster_ComboStep_TargetedFinished;
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_Attacking);
	// 모든 클라이언트가 이 태그를 받아 엘리트의 선딜 경고 색을 같은 시점에 표시한다.
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_TargetedAttacking);
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
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (HasSkillAuthority() && UASCombatTargetSubsystem::IsValidActorTarget(Target, Avatar, bCanHitPlayers)
		&& World)
	{
		if (UASCombatTargetSubsystem* CombatTargets = World->GetSubsystem<UASCombatTargetSubsystem>())
		{
			FASCombatTargetQuery Query;
			Query.SearchOrigin = Target->GetActorLocation();
			Query.SearchRadius = 1.0f;
			Query.Instigator = Avatar;
			Query.ExplicitActor = Target;
			Query.Domain = EASCombatTargetDomain::ActorOnly;
			Query.bCanHitPlayers = bCanHitPlayers;
			Query.MaxTargets = 1;
			const FGameplayEffectSpecHandle DamageSpec = MakeDamageSpec();
			FASCombatImpactContext ImpactContext;
			ImpactContext.Preset = ImpactPreset;
			ImpactContext.HitLocation = Target->GetActorLocation();
			ImpactContext.HitDirection = GetAimDirection();
			CombatTargets->ResolveAndApply(Query, MakeArrayView(&DamageSpec, 1), ImpactContext);
		}
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
