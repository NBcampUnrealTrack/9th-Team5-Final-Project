#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Items/GAS/ASItemWeaponAbility.h"
#include "PBWeaponAttackAbility.generated.h"

/** Normal weapon attack supplied by an equipped weapon. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UPBWeaponAttackAbility : public UASItemWeaponAbility
{
	GENERATED_BODY()

public:
	UPBWeaponAttackAbility();

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility,
		bool bWasCancelled) override;

private:
	void ComputeLocalAim(FVector& OutAimDirection, FVector& OutTargetLocation) const;
	void SendAimToServer(const FVector& AimDirection, const FVector& TargetLocation);
	void WaitForClientAim();
	void OnClientAimReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void OnClientAimCancelled();
	void ExecuteAttack(const FVector& AimDirection);

	bool bAttackStarted = false;
};
