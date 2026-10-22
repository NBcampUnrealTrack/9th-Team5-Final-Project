#pragma once

#include "CoreMinimal.h"
#include "Items/GAS/ASItemUseAbility.h"
#include "ASBarricadeUseAbility.generated.h"

class AASBarricadeActor;

/** Preview locally, confirm a position, then validate/place and consume on the server. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASBarricadeUseAbility : public UASItemUseAbility
{
	GENERATED_BODY()

public:
	UASBarricadeUseAbility();
	virtual bool RequiresUseEffects() const override { return false; }
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void ActivateItemUse(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Barricade")
	TSubclassOf<AASBarricadeActor> BarricadeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Item|Barricade", meta = (ClampMin = "1", Units = "s"))
	float TargetingTimeout = 30.0f;

private:
	void BeginTargeting();
	void PlaceAtLocation(const FVector& DesiredLocation);
	void UpdatePlacementPreview();
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
	TObjectPtr<AASBarricadeActor> PreviewActor;

	FTimerHandle PendingUseTimer;
	FDelegateHandle OtherAbilityActivatedHandle;
	TWeakObjectPtr<APawn> TargetingAvatar;
	double TargetingStartedAt = 0.0;
	bool bTargeting = false;
	bool bAddedTargetingTag = false;
	bool bSubmittedTarget = false;
};
