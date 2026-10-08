#pragma once

#include "CoreMinimal.h"
#include "Items/GAS/ASItemGameplayAbility.h"
#include "ASItemUseAbility.generated.h"

/** Server-only consumable. Blueprint subclasses must call CommitAbility before applying their effect. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASItemUseAbility : public UASItemGameplayAbility
{
	GENERATED_BODY()

public:
	UASItemUseAbility();
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
	mutable bool bItemCostPaid = false;
	mutable bool bApplyingItemCost = false;
};
