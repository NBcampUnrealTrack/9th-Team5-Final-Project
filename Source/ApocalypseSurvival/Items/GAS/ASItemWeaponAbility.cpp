#include "Items/GAS/ASItemWeaponAbility.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "Items/GAS/ASItemAbilityLibrary.h"

UASItemWeaponAbility::UASItemWeaponAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UASItemWeaponAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) return false;
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	return Spec && UASItemAbilityLibrary::CanCommitWeaponAttack(Spec->SourceObject.Get(), Cast<APawn>(ActorInfo->AvatarActor.Get()));
}

void UASItemWeaponAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || bAttackCostPaid) return;
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	bAttackCostPaid = Spec && UASItemAbilityLibrary::TryCommitWeaponAttack(
		Spec->SourceObject.Get(), Cast<APawn>(ActorInfo->AvatarActor.Get()));
	if (bAttackCostPaid) Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

bool UASItemWeaponAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	FGameplayTagContainer* OptionalRelevantTags)
{
	if (bAttackCostPaid) return false;
	return Super::CommitAbility(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags) && bAttackCostPaid;
}

void UASItemWeaponAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bAttackCostPaid = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}
