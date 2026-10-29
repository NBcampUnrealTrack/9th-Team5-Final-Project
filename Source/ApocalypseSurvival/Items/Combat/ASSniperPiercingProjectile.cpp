#include "Items/Combat/ASSniperPiercingProjectile.h"

#include "AbilitySystemComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "TimerManager.h"
#include "Components/SphereComponent.h"

void AASSniperPiercingProjectile::InitSniperPiercing(
	UASItemInstance* InWeapon, UAbilitySystemComponent* InAbilitySystem,
	const int32 InMaxPierceTargets, const TArray<float>& InDamageMultipliers, const float InTraceRadius)
{
	Weapon = InWeapon;
	AbilitySystem = InAbilitySystem;
	MaxPierceTargets = FMath::Max(1, InMaxPierceTargets);
	DamageMultipliers = InDamageMultipliers;
	if (DamageMultipliers.IsEmpty())
	{
		DamageMultipliers.Add(1.0f);
	}
	if (CollisionComponent)
	{
		CollisionComponent->SetSphereRadius(FMath::Max(1.0f, InTraceRadius));
	}
	bResolveSegmentHits = false;
}

void AASSniperPiercingProjectile::BeginPlay()
{
	Super::BeginPlay();
	PreviousPierceLocation = GetActorLocation();
}

void AASSniperPiercingProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bFinishResolved || IsActorBeingDestroyed() || HitCount >= MaxPierceTargets)
	{
		return;
	}
	ResolvePierceHits(GetActorLocation());
}

void AASSniperPiercingProjectile::ResolvePierceHits(const FVector& CurrentLocation)
{
	if (bFinishResolved || HitCount >= MaxPierceTargets)
	{
		return;
	}

	UASCombatTargetSubsystem* CombatTargets = GetWorld()
		? GetWorld()->GetSubsystem<UASCombatTargetSubsystem>()
		: nullptr;
	if (!CombatTargets)
	{
		PreviousPierceLocation = CurrentLocation;
		return;
	}
	if (CombatTargets->DeferUntilMassIdle(DeferredPierceHitTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			DeferredPierceHitTimer.Invalidate();
			ResolvePierceHits(GetActorLocation());
		})))
	{
		return;
	}

	UASItemInstance* CurrentWeapon = Weapon.Get();
	UAbilitySystemComponent* CurrentAbilitySystem = AbilitySystem.Get();
	const UASItemDefinition* Definition = CurrentWeapon ? CurrentWeapon->GetItemDefinition() : nullptr;
	if (!CurrentWeapon || !CurrentAbilitySystem || !Definition)
	{
		EndProjectile();
		return;
	}

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Segment;
	Query.SearchOrigin = PreviousPierceLocation;
	Query.EndLocation = CurrentLocation;
	Query.SearchRadius = CollisionComponent ? CollisionComponent->GetScaledSphereRadius() : 20.0f;
	Query.Instigator = InstigatorAvatar.Get();
	Query.bCanHitPlayers = CanHitPlayers();

	TArray<FASCombatTargetRef> Targets;
	CombatTargets->FindTargetsAlongSegment(Query, Targets);
	for (const FASCombatTargetRef& Target : Targets)
	{
		if (HitCount >= MaxPierceTargets)
		{
			break;
		}
		if (HasHitTarget(Target))
		{
			continue;
		}

		const float Multiplier = GetDamageMultiplier(HitCount);
		const float AdditionalDamage = Definition->BaseDamage * (Multiplier - 1.0f);
		const TArray<FGameplayEffectSpecHandle> pDamageSpecs = UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(
			CurrentWeapon, CurrentAbilitySystem, AdditionalDamage);
		if (pDamageSpecs.IsEmpty())
		{
			continue;
		}

		FASCombatImpactContext PerTargetImpact = ImpactContext;
		PerTargetImpact.HitLocation = CurrentLocation;
		const FASCombatApplyResult Result = CombatTargets->ApplyEffectSpecsToTarget(
			Target, pDamageSpecs, InstigatorAvatar.Get(), PerTargetImpact);
		if (!Result.HasAnyHit())
		{
			continue;
		}

		MarkHitTarget(Target);
		++HitCount;
	}

	PreviousPierceLocation = CurrentLocation;
	if (HitCount >= MaxPierceTargets)
	{
		EndProjectile();
	}
}

bool AASSniperPiercingProjectile::HasHitTarget(const FASCombatTargetRef& Target) const
{
	if (AActor* Actor = Target.GetActor())
	{
		return HitActors.Contains(TWeakObjectPtr<AActor>(Actor));
	}
	return Target.IsMass() && HitMassTargets.Contains(Target.GetMassTargetId());
}

void AASSniperPiercingProjectile::MarkHitTarget(const FASCombatTargetRef& Target)
{
	if (AActor* Actor = Target.GetActor())
	{
		HitActors.Add(TWeakObjectPtr<AActor>(Actor));
	}
	else if (Target.IsMass())
	{
		HitMassTargets.Add(Target.GetMassTargetId());
	}
}

float AASSniperPiercingProjectile::GetDamageMultiplier(const int32 HitOrder) const
{
	const int32 Index = FMath::Clamp(HitOrder, 0, DamageMultipliers.Num() - 1);
	return FMath::Clamp(DamageMultipliers[Index], 0.0f, 1.0f);
}

void AASSniperPiercingProjectile::OnProjectileFinishedNative()
{
	GetWorldTimerManager().ClearTimer(DeferredPierceHitTimer);
	Super::OnProjectileFinishedNative();
}
