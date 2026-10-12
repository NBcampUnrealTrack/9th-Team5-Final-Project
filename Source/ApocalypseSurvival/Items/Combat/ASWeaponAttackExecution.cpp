#include "Items/Combat/ASWeaponAttackExecution.h"

#include "AbilitySystemComponent.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "GameFramework/Character.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Skills/ASSkillAbility.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"

namespace ASWeaponAttackExecutionPrivate
{
	float GetStaminaCost(const FASWeaponAttackExecutionContext& Context)
	{
		const UASMeleeAttackProfile* Profile = Cast<UASMeleeAttackProfile>(Context.Profile);
		return Profile ? FMath::Max(0.0f, Profile->StaminaCost) : 0.0f;
	}

	UASItemDefinition* GetAmmoDefinition(const FASWeaponAttackExecutionContext& Context)
	{
		const UASRangedAttackProfile* Profile = Cast<UASRangedAttackProfile>(Context.Profile);
		return Profile ? Profile->AmmoDefinition.Get() : nullptr;
	}

	int32 GetAmmoPerShot(const FASWeaponAttackExecutionContext& Context)
	{
		const UASRangedAttackProfile* Profile = Cast<UASRangedAttackProfile>(Context.Profile);
		return Profile ? Profile->AmmoPerShot : 0;
	}

	void ApplyDamage(const FASWeaponAttackExecutionContext& Context, const TArray<AActor*>& Targets)
	{
		const TArray<FGameplayEffectSpecHandle> DamageSpecs = UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(
			Context.Weapon, Context.AbilitySystem);
		for (AActor* Target : Targets)
		{
			for (const FGameplayEffectSpecHandle& DamageSpec : DamageSpecs)
			{
				UASSkillAbility::ApplyDamageSpecToTarget(DamageSpec, Target);
			}
		}
	}

	void KeepClosestTarget(TArray<AActor*>& Targets, const FVector& ReferenceLocation)
	{
		if (Targets.Num() <= 1) return;
		AActor* ClosestTarget = nullptr;
		float ClosestDistanceSquared = TNumericLimits<float>::Max();
		for (AActor* Target : Targets)
		{
			if (!IsValid(Target)) continue;
			const float DistanceSquared = FVector::DistSquared2D(Target->GetActorLocation(), ReferenceLocation);
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestTarget = Target;
				ClosestDistanceSquared = DistanceSquared;
			}
		}
		Targets.Reset();
		if (ClosestTarget) Targets.Add(ClosestTarget);
	}

	void SpawnDirectionalProjectile(const FASWeaponAttackExecutionContext& Context, const UASRangedAttackProfile& Profile)
	{
		UWorld* World = Context.Avatar ? Context.Avatar->GetWorld() : nullptr;
		if (!World || !Context.Definition)
		{
			return;
		}

		FVector Direction = Context.AimDirection.GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = Context.Avatar->GetActorForwardVector().GetSafeNormal2D();
		}
		if (Direction.IsNearlyZero())
		{
			return;
		}

		FVector SpawnLocation = Context.Avatar->GetActorLocation()
			+ Direction * Profile.ProjectileSpawnForwardOffset
			+ FVector(0.0f, 0.0f, Profile.ProjectileSpawnHeightOffset);
		if (!Profile.ProjectileSpawnSocketName.IsNone())
		{
			if (const ACharacter* Character = Cast<ACharacter>(Context.Avatar))
			{
				if (const USkeletalMeshComponent* Mesh = Character->GetMesh(); Mesh && Mesh->DoesSocketExist(Profile.ProjectileSpawnSocketName))
				{
					SpawnLocation = Mesh->GetSocketLocation(Profile.ProjectileSpawnSocketName);
				}
			}
		}

		const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
		UClass* ProjectileClass = Profile.ProjectileClass
			? Profile.ProjectileClass.Get()
			: AASSkillProjectileActor::StaticClass();
		AASSkillProjectileActor* Projectile = World->SpawnActorDeferred<AASSkillProjectileActor>(
			ProjectileClass, SpawnTransform, Context.Avatar, Context.Avatar,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile)
		{
			return;
		}

		const TArray<FGameplayEffectSpecHandle> DamageSpecs = UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(
			Context.Weapon, Context.AbilitySystem);
		Projectile->InitProjectile(DamageSpecs, Context.Avatar,
			Direction, Profile.ProjectileSpeed, Context.Definition->AttackRange, Profile.ProjectilePierceCount, false);
		Projectile->FinishSpawning(SpawnTransform);
	}
}

bool UASMeleeAttackExecution::CanPayCost(const FASWeaponAttackExecutionContext& Context) const
{
	const UASMeleeAttackProfile* Profile = Cast<UASMeleeAttackProfile>(Context.Profile);
	return Profile
		&& Context.Definition && Context.Definition->WeaponType == EASWeaponType::Melee
		&& IsValid(Context.AbilitySystem)
		&& Profile->StaminaCostEffect
		&& Context.AbilitySystem->GetNumericAttribute(UASPlayerAttributeSet::GetStaminaAttribute())
			>= ASWeaponAttackExecutionPrivate::GetStaminaCost(Context);
}

bool UASMeleeAttackExecution::CommitCost(const FASWeaponAttackExecutionContext& Context) const
{
	if (!CanPayCost(Context) || !IsValid(Context.AbilitySystem) || !IsValid(Context.Weapon)) return false;
	const UASMeleeAttackProfile* Profile = Cast<UASMeleeAttackProfile>(Context.Profile);
	if (!Profile) return false;
	FGameplayEffectContextHandle EffectContext = Context.AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(Context.Weapon);
	const FGameplayEffectSpecHandle CostSpec = Context.AbilitySystem->MakeOutgoingSpec(
		Profile->StaminaCostEffect, 1.0f, EffectContext);
	if (!CostSpec.IsValid()) return false;
	const FGameplayTag StaminaCostTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.StaminaCost")), false);
	if (!StaminaCostTag.IsValid()) return false;
	CostSpec.Data->SetSetByCallerMagnitude(StaminaCostTag, -ASWeaponAttackExecutionPrivate::GetStaminaCost(Context));
	Context.AbilitySystem->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());

	if (ASWeaponAttackExecutionPrivate::GetStaminaCost(Context) > 0.0f
		&& Profile->StaminaRegenDelayEffect)
	{
		const FGameplayEffectSpecHandle DelaySpec = Context.AbilitySystem->MakeOutgoingSpec(
			Profile->StaminaRegenDelayEffect, 1.0f, EffectContext);
		if (DelaySpec.IsValid())
		{
			Context.AbilitySystem->ApplyGameplayEffectSpecToSelf(*DelaySpec.Data.Get());
		}
	}
	return true;
}

void UASMeleeAttackExecution::ExecuteHit(const FASWeaponAttackExecutionContext& Context) const
{
	if (!Context.Avatar || !Context.Definition || !Context.Avatar->GetWorld()) return;
	const UASMeleeAttackProfile* Profile = Cast<UASMeleeAttackProfile>(Context.Profile);
	if (!Profile) return;
	const float AttackRange = FMath::Max(1.0f, Context.Definition->AttackRange);
	const float DesiredRadius = Profile->HitRadius > 0.0f
		? Profile->HitRadius : FMath::Max(50.0f, AttackRange * 0.5f);
	const float Radius = FMath::Min(DesiredRadius, AttackRange);
	const float MaxOffset = FMath::Max(0.0f, AttackRange - Radius);
	const float Offset = Profile->HitboxOffset > 0.0f
		? FMath::Min(Profile->HitboxOffset, MaxOffset) : MaxOffset;
	const FVector Start = Context.Avatar->GetActorLocation();
	const FVector Center = Start + Context.AimDirection * Offset;
	TArray<AActor*> Targets = UASSkillAbility::FindSkillTargets(
		Context.Avatar->GetWorld(), Center, Radius, Context.Avatar, false);
	if (Profile->TargetMode == EASAttackTargetMode::Single)
	{
		AActor* RequestedTarget = Context.RequestedTarget.Get();
		const bool bRequestedTargetInRange = IsValid(RequestedTarget)
			&& UASSkillAbility::IsValidSkillTarget(RequestedTarget, Context.Avatar, false)
			&& FVector::DistSquared2D(Context.Avatar->GetActorLocation(), RequestedTarget->GetActorLocation())
			<= FMath::Square(AttackRange + 15.0f);
		if (bRequestedTargetInRange)
		{
			Targets.Reset();
			Targets.Add(RequestedTarget);
		}
		else
		{
			ASWeaponAttackExecutionPrivate::KeepClosestTarget(Targets, Center);
		}
	}
	ASWeaponAttackExecutionPrivate::ApplyDamage(Context, Targets);

	if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
	{
		Character->MulticastDrawWeaponAttackDebug(Start, Center, Radius, true, !Targets.IsEmpty());
	}
}

bool UASRangedAttackExecution::CanPayCost(const FASWeaponAttackExecutionContext& Context) const
{
	UASItemDefinition* Ammo = ASWeaponAttackExecutionPrivate::GetAmmoDefinition(Context);
	const int32 Quantity = ASWeaponAttackExecutionPrivate::GetAmmoPerShot(Context);
	return Cast<UASRangedAttackProfile>(Context.Profile)
		&& Context.Definition && Context.Definition->WeaponType == EASWeaponType::Ranged
		&& IsValid(Context.Inventory) && IsValid(Ammo) && Ammo->ItemType == EASItemType::Ammo
		&& Quantity > 0 && Context.Inventory->HasItem(Ammo, Quantity);
}

bool UASRangedAttackExecution::CommitCost(const FASWeaponAttackExecutionContext& Context) const
{
	UASItemDefinition* Ammo = ASWeaponAttackExecutionPrivate::GetAmmoDefinition(Context);
	const int32 Quantity = ASWeaponAttackExecutionPrivate::GetAmmoPerShot(Context);
	return CanPayCost(Context) && Context.Inventory->TryConsumeItem(Ammo, Quantity);
}

void UASRangedAttackExecution::ExecuteHit(const FASWeaponAttackExecutionContext& Context) const
{
	UWorld* World = Context.Avatar ? Context.Avatar->GetWorld() : nullptr;
	if (!World || !Context.Definition) return;
	const UASRangedAttackProfile* Profile = Cast<UASRangedAttackProfile>(Context.Profile);
	if (!Profile) return;
	const float TraceRadius = FMath::Max(0.0f, Profile->TraceRadius);
	const FVector Start = Context.Avatar->GetActorLocation();
	if (!Profile->bUseAttackMoveTargeting)
	{
		ASWeaponAttackExecutionPrivate::SpawnDirectionalProjectile(Context, *Profile);
		return;
	}

	if (Profile->TargetMode == EASAttackTargetMode::Single)
	{
		if (AActor* RequestedTarget = Context.RequestedTarget.Get(); IsValid(RequestedTarget))
		{
			const float AllowedRange = FMath::Max(0.0f, Context.Definition->AttackRange) + 15.0f;
			if (UASSkillAbility::IsValidSkillTarget(RequestedTarget, Context.Avatar, false)
				&& FVector::DistSquared2D(Context.Avatar->GetActorLocation(), RequestedTarget->GetActorLocation()) <= FMath::Square(AllowedRange))
			{
				TArray<AActor*> Targets;
				Targets.Add(RequestedTarget);
				ASWeaponAttackExecutionPrivate::ApplyDamage(Context, Targets);
				if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
				{
					Character->MulticastDrawWeaponAttackDebug(Start, RequestedTarget->GetActorLocation(), TraceRadius, false, true);
				}
				return;
			}
		}
	}

	const FVector End = Start + Context.AimDirection * FMath::Max(1.0f, Context.Definition->AttackRange);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponAttack), false, Context.Avatar);
	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(TraceRadius), QueryParams);

	AActor* FirstTarget = nullptr;
	float FirstHitTime = 1.0f;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (UASSkillAbility::IsValidSkillTarget(Target, Context.Avatar, false)
			&& (!FirstTarget || Hit.Time < FirstHitTime))
		{
			FirstTarget = Target;
			FirstHitTime = Hit.Time;
		}
	}

	TArray<AActor*> Targets;
	if (FirstTarget)
	{
		if (Profile->TargetMode == EASAttackTargetMode::Area)
		{
			const FVector ImpactCenter = Start + (End - Start) * FirstHitTime;
			Targets = UASSkillAbility::FindSkillTargets(
				World, ImpactCenter, FMath::Max(0.0f, Profile->AreaRadius), Context.Avatar, false);
			// The direct-hit target must always take damage even when its collision bounds
			// place the actor origin just outside a very small AreaRadius.
			Targets.AddUnique(FirstTarget);
		}
		else
		{
			Targets.Add(FirstTarget);
		}
	}
	ASWeaponAttackExecutionPrivate::ApplyDamage(Context, Targets);
	if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
	{
		const FVector DebugEnd = FirstTarget ? Start + (End - Start) * FirstHitTime : End;
		Character->MulticastDrawWeaponAttackDebug(Start, DebugEnd, TraceRadius, false, FirstTarget != nullptr);
	}
}
