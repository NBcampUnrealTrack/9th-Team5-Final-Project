#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterPatternAbility.h"
#include "ASMonsterSniperBeamAbility.generated.h"

class AASMonsterBeamTelegraphActor;

/** 추적 조준 후 방향을 고정하고 일직선 강격을 발사하는 엘리트 B 전용 패턴. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterSniperBeamAbility : public UASMonsterPatternAbility
{
	GENERATED_BODY()

public:
	UASMonsterSniperBeamAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SniperBeam")
	TSubclassOf<AASMonsterBeamTelegraphActor> TelegraphClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SniperBeam", meta = (ClampMin = "0.05", Units = "s"))
	float TrackingDuration = 1.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SniperBeam", meta = (ClampMin = "0.05", Units = "s"))
	float LockDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SniperBeam", meta = (ClampMin = "0.02", Units = "s"))
	float TrackingUpdateInterval = 1.0f / 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SniperBeam", meta = (ClampMin = "1.0", Units = "cm"))
	float BeamLength = 2200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SniperBeam", meta = (ClampMin = "1.0", Units = "cm"))
	float BeamWidth = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SniperBeam", meta = (Units = "cm"))
	float MuzzleForwardOffset = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SniperBeam", meta = (Units = "cm"))
	float MuzzleHeightOffset = 70.0f;

private:
	bool LockTarget();
	void UpdateTrackingAim();
	void LockAim();
	void FireBeam();
	void FinishBeam(bool bWasCancelled);
	void DestroyTelegraph();
	FVector GetMuzzleLocation() const;
	float GetBlockedBeamLength(const FVector& Start, const FVector& Direction) const;

	TWeakObjectPtr<AActor> LockedTarget;
	TWeakObjectPtr<AASMonsterBeamTelegraphActor> Telegraph;
	FVector LockedDirection = FVector::ForwardVector;
	FVector LockedStartLocation = FVector::ZeroVector;
	float LockedLength = 0.0f;
	bool bAimLocked = false;
	FTimerHandle TrackingTimerHandle;
	FTimerHandle LockTimerHandle;
	FTimerHandle FireTimerHandle;
};
