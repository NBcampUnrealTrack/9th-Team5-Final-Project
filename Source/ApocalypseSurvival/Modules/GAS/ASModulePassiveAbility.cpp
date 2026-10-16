#include "Modules/GAS/ASModulePassiveAbility.h"

#include "GAS/ASAbilityTags.h"
#include "Modules/ASModuleComponent.h"

UASModulePassiveAbility::UASModulePassiveAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Dead);
}

bool UASModulePassiveAbility::GetModuleCoreValue(float& OutValue) const
{
	OutValue = 0.0f;
	const UASModuleComponent* Modules = Cast<UASModuleComponent>(GetCurrentSourceObject());
	return Modules && Modules->GetAbilityCoreValue(GetCurrentAbilitySpecHandle(), OutValue);
}
