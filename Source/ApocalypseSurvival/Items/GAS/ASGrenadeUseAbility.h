#pragma once

#include "CoreMinimal.h"
#include "Items/GAS/ASItemUseAbility.h"
#include "ASGrenadeUseAbility.generated.h"

class AASGrenadeActor;
class AASSkillRangeIndicatorActor;

/** Cursor-targeted consumable. Shows the blast radius, waits for left-click, then commits inventory cost on the server. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASGrenadeUseAbility : public UASItemUseAbility
{
	GENERATED_BODY()

public:
	UASGrenadeUseAbility();
	virtual bool RequiresUseEffects() const override { return false; }
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void ActivateItemUse(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade")
	TSubclassOf<AASGrenadeActor> GrenadeClass;

	/** Optional visual override. Empty reuses the equipped character's E skill indicator, then another configured skill. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Targeting")
	TSubclassOf<AASSkillRangeIndicatorActor> RangeIndicatorClass;

	/** Ballistic flight time to the selected ground point when unobstructed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade", meta = (ClampMin = "0.05", Units = "s"))
	float ThrowFlightTime = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Targeting", meta = (ClampMin = "1.0", Units = "s"))
	float TargetingTimeout = 30.0f;

	/** Offset relative to horizontal facing. Grenades are thrown from near the avatar's upper body. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade", meta = (Units = "cm"))
	FVector SpawnOffset = FVector(50.0f, 0.0f, 30.0f);

private:
	void BeginTargeting();
	void ThrowAtLocation(const FVector& DesiredLocation);
	void StopTargeting();
	void CheckPendingUse();
	void OnOtherAbilityActivated(UGameplayAbility* Ability);
	void OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void OnTargetDataCancelled();

	UFUNCTION()
	void OnLocalConfirm();

	UFUNCTION()
	void OnLocalCancel();

	UPROPERTY(Transient)
	TObjectPtr<AASSkillRangeIndicatorActor> RangeIndicator;

	FTimerHandle PendingUseTimer;
	FDelegateHandle OtherAbilityActivatedHandle;
	TWeakObjectPtr<APawn> TargetingAvatar;
	double TargetingStartedAt = 0.0;
	bool bTargeting = false;
	bool bAddedTargetingTag = false;
	bool bSubmittedTarget = false;
};
