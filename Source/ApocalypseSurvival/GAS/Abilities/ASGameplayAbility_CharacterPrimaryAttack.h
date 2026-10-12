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

	/** 현재 장착 무기의 일반 공격 사거리. 타게팅 인디케이터 표시에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Attack|Targeting")
	float GetCurrentAttackRange() const;

	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputPressed(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicatedEndAbility, bool bWasCancelled) override;

private:
	void BeginTargeting();
	void ConfirmTargeting();
	void StopTargetingInput(bool bWasCancelled);

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

	/** State.Targeting 중 좌클릭이 들어오면 ASC가 호출한다. */
	UFUNCTION()
	void OnLocalInputConfirm();

protected:
	/** BP 자식은 이 훅에서 범위/커서 인디케이터를 표시한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Attack|Targeting")
	void OnAttackTargetingStarted();

	/** BP 자식은 이 훅에서 범위/커서 인디케이터를 숨긴다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Attack|Targeting")
	void OnAttackTargetingEnded(bool bWasCancelled);

private:
	bool bIsTargeting = false;
	bool bAddedTargetingTag = false;
	bool bBoundLocalConfirm = false;
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
