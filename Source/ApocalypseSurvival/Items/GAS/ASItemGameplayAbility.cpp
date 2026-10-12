#include "Items/GAS/ASItemGameplayAbility.h"

#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"

UASItemGameplayAbility::UASItemGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UASItemInstance* UASItemGameplayAbility::GetSourceItem() const
{
	UASItemInstance* CurrentItem = UASItemAbilityLibrary::GetItemFromSourceObject(GetCurrentSourceObject());
	return CurrentItem ? CurrentItem : ActiveSourceItem.Get();
}

UASItemDefinition* UASItemGameplayAbility::GetSourceItemDefinition() const
{
	return UASItemAbilityLibrary::GetItemDefinitionFromSourceObject(GetSourceItem());
}

float UASItemGameplayAbility::GetItemBaseDamage() const
{
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	return IsValid(Definition) ? Definition->BaseDamage : 0.0f;
}

float UASItemGameplayAbility::GetWeaponAttackInterval() const
{
	const UASItemDefinition* Definition = UASItemAbilityLibrary::GetWeaponDefinitionFromSourceObject(GetSourceItem());
	return Definition ? FMath::Max(0.01f, Definition->AttackInterval) : 0.0f;
}

float UASItemGameplayAbility::GetWeaponAttackRange() const
{
	const UASItemDefinition* Definition = UASItemAbilityLibrary::GetWeaponDefinitionFromSourceObject(GetSourceItem());
	return Definition ? FMath::Max(0.0f, Definition->AttackRange) : 0.0f;
}

void UASItemGameplayAbility::CacheSourceItem()
{
	ActiveSourceItem = UASItemAbilityLibrary::GetItemFromSourceObject(GetCurrentSourceObject());
}

void UASItemGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	CacheSourceItem();
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}
