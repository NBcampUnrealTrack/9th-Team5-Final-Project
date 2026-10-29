#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Items/Combat/ASWeaponAttackExecution.h"
#include "ASGameplayAbility_CharacterPrimaryAttack.generated.h"

class UAnimMontage;
class UASItemDefinition;
class UASWeaponAttackExecution;
class AActor;

/** Character-owned primary attack. Equipment supplies data; it never grants or removes this ability. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASGameplayAbility_CharacterPrimaryAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASGameplayAbility_CharacterPrimaryAttack();

	/** Attack Move를 시작해야 하는 GA인지 판단한다. Controller가 방향 공격 GA를 제외할 때 사용한다. */
	virtual bool IsAttackMoveAbility() const { return true; }

	/** 현재 장착 무기의 일반 공격 사거리. 타게팅 인디케이터 표시에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Attack|Targeting")
	float GetCurrentAttackRange() const;

	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputPressed(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicatedEndAbility, bool bWasCancelled) override;

protected:
	/** 자식 GA는 조준 방식과 조준 상태 태그만 바꾼다. 실제 공격 실행은 이 클래스를 공통으로 사용한다. */
	virtual EASPrimaryAttackMode GetAttackMode() const { return EASPrimaryAttackMode::AttackMove; }
	virtual FGameplayTag GetTargetingStateTag() const;
	virtual bool ShouldConsumeAttackMoveRequest() const { return true; }

private:
	FTimerHandle DeferredHitTimer;
	FTimerHandle DeferredFinishTimer;

	void BeginTargeting();
	void ConfirmTargeting();
	void StopTargetingInput(bool bWasCancelled);

	bool BuildExecutionContext(FASWeaponAttackExecutionContext& OutContext,
		const UASWeaponAttackExecution*& OutExecution) const;
	void ResolveAnimation(const FASWeaponAttackExecutionContext& Context, UAnimMontage*& OutMontage,
		FGameplayTag& OutHitEventTag, bool& bOutScaleToInterval) const;
	void ComputeLocalAim(FVector& OutAimDirection, FVector& OutTargetLocation) const;
	void SendAimToServer(const FVector& AimDirection, const FVector& TargetLocation, AActor* RequestedTarget,
		uint32 RequestedMassTargetNetworkId);
	void WaitForClientAim();
	void OnClientAimReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void OnClientAimCancelled();
	void StartAttack(const FVector& AimDirection, const FVector& TargetLocation, AActor* RequestedTarget,
		uint32 RequestedMassTargetNetworkId);
	bool IsValidAttackRequest(const FASWeaponAttackExecutionContext& Context, AActor* RequestedTarget,
		uint32 RequestedMassTargetNetworkId, const FVector& TargetLocation, FVector& OutAimDirection) const;
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

	UFUNCTION()
	void OnLocalInputCancel();

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
	bool bBoundLocalCancel = false;
	bool bAttackStarted = false;
	bool bHitPerformed = false;
	double NextAttackTime = 0.0;
	FVector CachedAimDirection = FVector::ForwardVector;
	TWeakObjectPtr<AActor> CachedRequestedTarget;
	FASWeaponAttackExecutionContext CachedExecutionContext;
	const UASWeaponAttackExecution* CachedExecution = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;
	FGameplayTag ActiveHitEventTag;
	FGameplayTag ActiveTargetingStateTag;
};
