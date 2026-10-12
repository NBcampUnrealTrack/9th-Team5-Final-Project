#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Items/Combat/ASWeaponAttackExecution.h"
#include "ASGameplayAbility_CharacterPrimaryAttack.generated.h"

class UAnimMontage;
class UASItemDefinition;
class UASWeaponAttackExecution;

/** Character-owned primary attack. Equipment supplies data; it never grants or removes this ability. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASGameplayAbility_CharacterPrimaryAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASGameplayAbility_CharacterPrimaryAttack();

	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicatedEndAbility, bool bWasCancelled) override;

private:
	bool BuildExecutionContext(FASWeaponAttackExecutionContext& OutContext,
		const UASWeaponAttackExecution*& OutExecution) const;
	void ResolveAnimation(const FASWeaponAttackExecutionContext& Context, UAnimMontage*& OutMontage,
		FGameplayTag& OutHitEventTag, bool& bOutScaleToInterval) const;
	void ComputeLocalAim(FVector& OutAimDirection, FVector& OutTargetLocation) const;
	void SendAimToServer(const FVector& AimDirection, const FVector& TargetLocation);
	void WaitForClientAim();
	void OnClientAimReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void OnClientAimCancelled();
	void StartAttack(const FVector& AimDirection);
	void PerformHit();
	float GetMontagePlayRate(const UAnimMontage* Montage, const UASItemDefinition* Definition,
		bool bScaleToInterval) const;

	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

	bool bAttackStarted = false;
	bool bHitPerformed = false;
	double NextAttackTime = 0.0;
	FVector CachedAimDirection = FVector::ForwardVector;
	FASWeaponAttackExecutionContext CachedExecutionContext;
	const UASWeaponAttackExecution* CachedExecution = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;
	FGameplayTag ActiveHitEventTag;
};
