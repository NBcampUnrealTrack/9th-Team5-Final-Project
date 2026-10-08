#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ASItemGameplayAbility.generated.h"

class UASItemDefinition;
class UASItemInstance;

/** Shared source-object access for weapon and consumable abilities granted by inventory. */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASItemGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASItemGameplayAbility();

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	UASItemInstance* GetSourceItem() const;

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	UASItemDefinition* GetSourceItemDefinition() const;

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	float GetItemBaseDamage() const;

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	float GetItemHealAmount() const;

	/** Returns zero when this ability has no weapon source. */
	UFUNCTION(BlueprintPure, Category = "Item|GAS|Weapon")
	float GetWeaponAttackInterval() const;

	UFUNCTION(BlueprintPure, Category = "Item|GAS|Weapon")
	float GetWeaponAttackRange() const;

protected:
	/** Keep the last consumed unit available to delayed Blueprint use effects. */
	void CacheSourceItem();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UASItemInstance> ActiveSourceItem;
};
