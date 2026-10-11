#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Items/GAS/ASItemWeaponAbility.h"
#include "PBWeaponAttackAbility.generated.h"

class UAnimMontage;
class UASItemDefinition;

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

protected:
	/** 공격 몽타주. 비우면 기존처럼 즉시 판정한다. 무기별 BP 자식에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 판정 타이밍 이벤트. 몽타주의 AS Skill Event 노티파이가 보내는 태그와 같아야 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation", meta = (Categories = "Event"))
	FGameplayTag HitEventTag;

	/** true면 몽타주가 무기의 AttackInterval보다 길 때 그 간격에 맞춰 빠르게 재생한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	bool bScaleMontageToAttackInterval = true;

private:
	void ComputeLocalAim(FVector& OutAimDirection, FVector& OutTargetLocation) const;
	void SendAimToServer(const FVector& AimDirection, const FVector& TargetLocation);
	void WaitForClientAim();
	void OnClientAimReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void OnClientAimCancelled();
	/** 서버는 Commit 후, 클라이언트는 바로 몽타주를 재생한다. 몽타주가 없으면 즉시 판정한다. */
	void StartAttack(const FVector& AimDirection);

	/** 서버 전용. 근접·원거리 판정과 데미지 적용. */
	void PerformHit(const FVector& AimDirection);

	float GetMontagePlayRate(const UASItemDefinition* Definition) const;

	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

	bool bAttackStarted = false;
	bool bHitPerformed = false;
	FVector CachedAimDirection = FVector::ForwardVector;
};
