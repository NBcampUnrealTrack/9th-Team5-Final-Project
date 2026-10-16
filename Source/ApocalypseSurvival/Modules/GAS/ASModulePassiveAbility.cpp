#include "Modules/GAS/ASModulePassiveAbility.h"

#include "GAS/ASAbilityTags.h"

UASModulePassiveAbility::UASModulePassiveAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Dead);
}
