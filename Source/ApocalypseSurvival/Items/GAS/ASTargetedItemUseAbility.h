#pragma once

#include "CoreMinimal.h"
#include "Items/GAS/ASItemUseAbility.h"
#include "ASTargetedItemUseAbility.generated.h"

/** Shared cursor-targeted consumable lifecycle. Concrete abilities own preview visuals and execution. */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASTargetedItemUseAbility : public UASItemUseAbility
{
	GENERATED_BODY()
public:
	UASTargetedItemUseAbility();
	virtual bool RequiresUseEffects() const override { return false; }
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	void StartTargeting(float Timeout, float UpdateInterval, bool bRecheckCost);
	void BindTargetingInput();
	virtual void BeginTargeting() PURE_VIRTUAL(UASTargetedItemUseAbility::BeginTargeting, );
	virtual float GetTargetRange() const PURE_VIRTUAL(UASTargetedItemUseAbility::GetTargetRange, return 0.0f;);
	virtual bool IsTargetLocationValid(const FVector& Location) const { return true; }
	virtual void ExecuteAtLocation(const FVector& Location) PURE_VIRTUAL(UASTargetedItemUseAbility::ExecuteAtLocation, );
	virtual void UpdateTargetingPreview() {}
	virtual void CleanupTargetingVisuals() {}

	void OnTargetDataCancelled();
	UFUNCTION()
	void OnLocalCancel();

	TWeakObjectPtr<APawn> TargetingAvatar;

private:
	void StopTargeting();
	void CheckPendingUse();
	void OnOtherAbilityActivated(UGameplayAbility* Ability);
	void OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void SubmitTargetLocation(const FVector& Location);
	UFUNCTION()
	void OnLocalConfirm();

	FTimerHandle PendingUseTimer;
	FDelegateHandle OtherAbilityActivatedHandle;
	double TargetingStartedAt = 0.0;
	float ActiveTargetingTimeout = 0.0f;
	bool bTargeting = false;
	bool bAddedTargetingTag = false;
	bool bSubmittedTarget = false;
	bool bRecheckPendingCost = false;
};
