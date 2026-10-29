#pragma once

#include "CoreMinimal.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"
#include "ASSniperPiercingProjectile.generated.h"

class UAbilitySystemComponent;
class UASItemInstance;

/**
 * Server-authoritative sniper projectile. It finds every newly intersected target in travel order,
 * applies a separate scaled damage spec to each, and never hits the same Actor/Mass target twice.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASSniperPiercingProjectile : public AASSkillProjectileActor
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;

	void InitSniperPiercing(UASItemInstance* InWeapon, UAbilitySystemComponent* InAbilitySystem,
		int32 InMaxPierceTargets, const TArray<float>& InDamageMultipliers, float InTraceRadius);

protected:
	virtual void BeginPlay() override;
	virtual void OnProjectileFinishedNative() override;

private:
	void ResolvePierceHits(const FVector& CurrentLocation);
	bool HasHitTarget(const FASCombatTargetRef& Target) const;
	void MarkHitTarget(const FASCombatTargetRef& Target);
	float GetDamageMultiplier(int32 HitOrder) const;

	TWeakObjectPtr<UASItemInstance> Weapon;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	TSet<uint64> HitMassTargets;
	TArray<float> DamageMultipliers;
	FVector PreviousPierceLocation = FVector::ZeroVector;
	FTimerHandle DeferredPierceHitTimer;
	int32 MaxPierceTargets = 1;
	int32 HitCount = 0;
	bool bFinishResolved = false;
};
