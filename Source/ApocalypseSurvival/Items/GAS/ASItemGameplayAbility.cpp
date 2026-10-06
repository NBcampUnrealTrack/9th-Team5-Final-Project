#include "Items/GAS/ASItemGameplayAbility.h"

#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"

UASItemGameplayAbility::UASItemGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UASItemInstance* UASItemGameplayAbility::GetSourceItem() const
{
	return Cast<UASItemInstance>(GetCurrentSourceObject());
}

UASItemDefinition* UASItemGameplayAbility::GetSourceItemDefinition() const
{
	const UASItemInstance* Item = GetSourceItem();
	return IsValid(Item) ? Item->GetItemDefinition() : nullptr;
}

float UASItemGameplayAbility::GetItemBaseDamage() const
{
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	return IsValid(Definition) ? Definition->BaseDamage : 0.0f;
}

float UASItemGameplayAbility::GetItemHealAmount() const
{
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	return IsValid(Definition) ? Definition->HealAmount : 0.0f;
}
