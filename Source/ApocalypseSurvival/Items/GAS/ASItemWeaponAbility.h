#pragma once

#include "CoreMinimal.h"
#include "Items/GAS/ASItemGameplayAbility.h"
#include "ASItemWeaponAbility.generated.h"

/** Optional base for one server-authoritative attack per activation.
 * Combat subclasses prepare/validate their attack, call CommitAbility, execute only on success,
 * and EndAbility. Commit handles equipped-source validation, interval and inventory ammo.
 * Existing skill bases can instead use ASItemAbilityLibrary's attack commit functions.
 */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASItemWeaponAbility : public UASItemGameplayAbility
{
	GENERATED_BODY()
public:
	UASItemWeaponAbility();
	virtual bool CommitAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
	virtual bool CheckCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;
protected:
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	mutable bool bAttackCostPaid = false;
};
