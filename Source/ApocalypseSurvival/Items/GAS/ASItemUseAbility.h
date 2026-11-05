#pragma once

#include "CoreMinimal.h"
#include "Items/GAS/ASItemGameplayAbility.h"
#include "Items/Data/ASItemAssetLoading.h"
#include "ASItemUseAbility.generated.h"

/** Server-only consumable. Custom use actions must call CommitAbility before applying their effect. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASItemUseAbility : public UASItemGameplayAbility
{
	GENERATED_BODY()

public:
	UASItemUseAbility();
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	/** Native custom actions override this to false and implement ActivateItemUse instead of ActivateAbility. */
	virtual bool RequiresUseEffects() const;

	virtual bool CommitAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
	virtual bool CheckCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Called after per-use cost state and source item are initialized. Custom actions must commit and end the ability. */
	virtual void ActivateItemUse(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData);

private:
	void StartPreparedUse(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData);
	FASItemAssetRequest UseAssetRequest;
	UPROPERTY(Transient) FGameplayEventData PendingUseEvent;
	bool bHasPendingUseEvent = false;
	uint64 PreparationSerial = 0;
	mutable bool bItemCostPaid = false;
	mutable bool bApplyingItemCost = false;
};
