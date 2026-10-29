#pragma once

#include "CoreMinimal.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"
#include "ASShotgunAttackSequence.generated.h"

class APawn;
class UAbilitySystemComponent;
class UASItemInstance;
class UASShotgunAttackProfile;
struct FASWeaponAttackExecutionContext;

UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASShotgunPelletProjectile : public AASSkillProjectileActor
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	void InitShotgunPellet(class AASShotgunAttackSequence* InSequence, int32 InPelletIndex);

protected:
	virtual void BeginPlay() override;
	virtual void OnProjectileFinishedNative() override;

private:
	void ResolvePelletHit(const FVector& CurrentLocation);
	void NotifyFinished();

	TWeakObjectPtr<AASShotgunAttackSequence> Sequence;
	int32 PelletIndex = INDEX_NONE;
	FVector PreviousPelletLocation = FVector::ZeroVector;
	FTimerHandle DeferredPelletHitTimer;
	bool bPelletResolved = false;
	bool bFinishNotified = false;
};

/** Server-only owner for a single shotgun blast and its per-target knockback ledger. */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class APOCALYPSESURVIVAL_API AASShotgunAttackSequence : public AActor
{
	GENERATED_BODY()

public:
	AASShotgunAttackSequence();
	void InitShotgun(const FASWeaponAttackExecutionContext& Context, const UASShotgunAttackProfile* InProfile);

	FASCombatImpactContext BuildImpactContextForPotentialHit(
		const FASCombatTargetRef& Target, const FASCombatImpactContext& BaseContext) const;
	void NotifyPelletHit(const FASCombatTargetRef& Target, const FASCombatImpactContext& AppliedImpactContext);
	void NotifyPelletFinished();

protected:
	virtual void BeginPlay() override;

private:
	FVector ResolveBaseDirection() const;
	FVector ResolveSpawnLocation(const FVector& Direction) const;
	FASCombatImpactContext MakeBaseImpactContext(const FVector& SpawnLocation, const FVector& Direction,
		int32 PelletIndex) const;
	bool HasAppliedKnockback(const FASCombatTargetRef& Target) const;
	void SpawnPellets();
	void CommitKnockback();
	void TryFinishSequence();

	struct FPendingKnockback
	{
		FASCombatTargetRef Target;
		FASCombatImpactContext ImpactContext;
	};

	TWeakObjectPtr<APawn> Avatar;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UASItemInstance> Weapon;
	TWeakObjectPtr<UASShotgunAttackProfile> Profile;
	uint8 AttackModeValue = 0;
	FVector AimDirection = FVector::ForwardVector;
	int32 ActivePelletCount = 0;
	bool bInitialized = false;
	bool bFinishedSpawning = false;
	bool bKnockbackPending = false;
	bool bKnockbackCommitted = false;
	FTimerHandle KnockbackTimerHandle;
	TSet<TWeakObjectPtr<AActor>> KnockbackActors;
	TSet<uint64> KnockbackMassTargets;
	TArray<FPendingKnockback> PendingKnockbacks;
};
