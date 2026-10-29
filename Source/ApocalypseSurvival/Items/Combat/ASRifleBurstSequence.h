#pragma once

#include "CoreMinimal.h"
#include "Items/Combat/ASWeaponProjectile.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"
#include "ASRifleBurstSequence.generated.h"

class APawn;
class UAbilitySystemComponent;
class UASInventoryComponent;
class UASItemInstance;
class UASRifleBurstAttackProfile;
struct FASWeaponAttackExecutionContext;

UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASRifleBurstDirectionalProjectile : public AASSkillProjectileActor
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	void InitBurstShot(class AASRifleBurstSequence* InSequence, int32 InShotIndex);

protected:
	virtual void BeginPlay() override;
	virtual void OnProjectileFinishedNative() override;

private:
	void ResolveBurstHit(const FVector& CurrentLocation);
	void NotifyFinished();

	TWeakObjectPtr<AASRifleBurstSequence> Sequence;
	int32 ShotIndex = INDEX_NONE;
	FVector PreviousBurstLocation = FVector::ZeroVector;
	FTimerHandle DeferredBurstHitTimer;
	bool bBurstResolved = false;
	bool bFinishNotified = false;
};

UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASRifleBurstHomingProjectile : public AASWeaponProjectile
{
	GENERATED_BODY()

public:
	void InitBurstShot(AASRifleBurstSequence* InSequence, int32 InShotIndex,
		const FASCombatTargetRef& InLockedTarget);

protected:
	virtual FASCombatImpactContext ResolveImpactContextForTarget(
		const FASCombatTargetRef& InTarget, const FASCombatImpactContext& BaseContext) const override;
	virtual void OnHomingProjectileFinished(bool bHit) override;

private:
	TWeakObjectPtr<AASRifleBurstSequence> Sequence;
	FASCombatTargetRef LockedTarget;
	int32 ShotIndex = INDEX_NONE;
	bool bFinishNotified = false;
};

/**
 * Server-only lifetime owner for one paid rifle attack. It spaces shots, revalidates the equipped
 * weapon before every spawn, and remembers confirmed hits until all spawned projectiles finish.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class APOCALYPSESURVIVAL_API AASRifleBurstSequence : public AActor
{
	GENERATED_BODY()

public:
	AASRifleBurstSequence();
	void InitBurst(const FASWeaponAttackExecutionContext& Context, const UASRifleBurstAttackProfile* InProfile);

	FASCombatImpactContext BuildImpactContextForPotentialHit(
		const FASCombatTargetRef& Target, const FASCombatImpactContext& BaseContext, int32 ShotIndex) const;
	void NotifyProjectileHit(const FASCombatTargetRef& Target);
	void NotifyProjectileFinished();

protected:
	virtual void BeginPlay() override;

private:
	bool IsBurstStillValid() const;
	bool FindLockedTarget(FASCombatTargetRef& OutTarget) const;
	FVector ResolveAimDirection() const;
	FVector ResolveSpawnLocation(const FVector& Direction) const;
	FASCombatImpactContext MakeBaseImpactContext(const FVector& SpawnLocation, const FVector& Direction,
		int32 ShotIndex) const;
	bool SpawnDirectionalShot(int32 ShotIndex);
	bool SpawnHomingShot(int32 ShotIndex);
	void FireNextShot();
	void StopSpawning();
	void TryFinishSequence();
	int32 GetConfirmedHitCount(const FASCombatTargetRef& Target) const;

	TWeakObjectPtr<APawn> Avatar;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UASInventoryComponent> Inventory;
	TWeakObjectPtr<UASItemInstance> Weapon;
	TWeakObjectPtr<UASRifleBurstAttackProfile> Profile;
	TWeakObjectPtr<AActor> RequestedTarget;
	uint32 RequestedMassTargetNetworkId = 0;
	FVector AimDirection = FVector::ForwardVector;
	FVector TargetLocation = FVector::ZeroVector;
	uint8 AttackModeValue = 0;
	int32 NextShotIndex = 0;
	int32 ActiveProjectileCount = 0;
	bool bInitialized = false;
	bool bFinishedSpawning = false;
	FTimerHandle ShotTimerHandle;
	TMap<TWeakObjectPtr<AActor>, int32> ConfirmedActorHits;
	TMap<uint64, int32> ConfirmedMassHits;
};
