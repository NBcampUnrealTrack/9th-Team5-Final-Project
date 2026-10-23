#include "Items/Combat/ASWeaponAttackExecution.h"

#include "AbilitySystemComponent.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "GameFramework/Character.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/Inventory/ASInventoryComponent.h"
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

	TArray<FGameplayEffectSpecHandle> MakeDamageSpecs(const FASWeaponAttackExecutionContext& Context)
	{
		return UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(Context.Weapon, Context.AbilitySystem);
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

		const TArray<FGameplayEffectSpecHandle> DamageSpecs = MakeDamageSpecs(Context);
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
	const TArray<FGameplayEffectSpecHandle> DamageSpecs = ASWeaponAttackExecutionPrivate::MakeDamageSpecs(Context);
	UASCombatTargetSubsystem* CombatTargets =
		Context.Avatar->GetWorld()->GetSubsystem<UASCombatTargetSubsystem>();
	if (!CombatTargets)
	{
		return;
	}

	FASCombatTargetQuery Query;
	Query.SearchOrigin = Center;
	Query.SearchRadius = Radius;
	Query.Instigator = Context.Avatar;
	if (Profile->TargetMode == EASAttackTargetMode::Single)
	{
		AActor* RequestedTarget = Context.RequestedTarget.Get();
		const float AllowedRange = AttackRange + 15.0f;
		const bool bRequestedTargetInRange = IsValid(RequestedTarget)
			&& UASCombatTargetSubsystem::IsValidActorTarget(RequestedTarget, Context.Avatar, false)
			&& FVector::DistSquared2D(Context.Avatar->GetActorLocation(), RequestedTarget->GetActorLocation())
			<= FMath::Square(AllowedRange);
		if (bRequestedTargetInRange)
		{
			Query.Domain = EASCombatTargetDomain::ActorOnly;
			Query.ExplicitActor = RequestedTarget;
			Query.SearchOrigin = RequestedTarget->GetActorLocation();
			Query.SearchRadius = 1.0f;
		}
		else
		{
			// Mass 단일 공격은 넓은 피격 범위에서 첫 Entity를 꺼내지 않는다.
			// 클릭이 사거리 안이면 클릭점 기준, 밖이면 공격자 기준으로 하나를 고른 뒤
			// ResolveAndApplyBestTarget이 그 Entity ID에만 피해를 적용한다.
			const bool bClickIsInAttackRange = FVector::DistSquared2D(Start, Context.TargetLocation)
				<= FMath::Square(AllowedRange);
			Query.SearchOrigin = Start;
			Query.SearchRadius = AllowedRange;
			Query.Domain = EASCombatTargetDomain::MassOnly;
			Query.SortPolicy = bClickIsInAttackRange
				? EASCombatTargetSortPolicy::NearestToPriorityLocation
				: EASCombatTargetSortPolicy::NearestToSearchOrigin;
			Query.PriorityLocation = Context.TargetLocation;
		}
		Query.MaxTargets = 1;
	}
	const FASCombatApplyResult ApplyResult = Profile->TargetMode == EASAttackTargetMode::Single
		? CombatTargets->ResolveAndApplyBestTarget(Query, DamageSpecs)
		: CombatTargets->ResolveAndApply(Query, DamageSpecs);

	if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
	{
		Character->MulticastDrawWeaponAttackDebug(Start, Center, Radius, true,
			ApplyResult.HasAnyHit());
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
	const TArray<FGameplayEffectSpecHandle> DamageSpecs = ASWeaponAttackExecutionPrivate::MakeDamageSpecs(Context);
	UASCombatTargetSubsystem* CombatTargets = World->GetSubsystem<UASCombatTargetSubsystem>();
	if (!CombatTargets)
	{
		return;
	}

	if (Profile->TargetMode == EASAttackTargetMode::Single)
	{
		if (AActor* RequestedTarget = Context.RequestedTarget.Get(); IsValid(RequestedTarget))
		{
			const float AllowedRange = FMath::Max(0.0f, Context.Definition->AttackRange) + 15.0f;
			if (UASCombatTargetSubsystem::IsValidActorTarget(RequestedTarget, Context.Avatar, false)
				&& FVector::DistSquared2D(Context.Avatar->GetActorLocation(), RequestedTarget->GetActorLocation()) <= FMath::Square(AllowedRange))
			{
				FASCombatTargetQuery Query;
				Query.SearchOrigin = RequestedTarget->GetActorLocation();
				Query.SearchRadius = 1.0f;
				Query.Instigator = Context.Avatar;
				Query.Domain = EASCombatTargetDomain::ActorOnly;
				Query.ExplicitActor = RequestedTarget;
				Query.MaxTargets = 1;
				CombatTargets->ResolveAndApplyBestTarget(Query, DamageSpecs);
				if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
				{
					Character->MulticastDrawWeaponAttackDebug(Start, RequestedTarget->GetActorLocation(), TraceRadius, false, true);
				}
				return;
			}
		}

		// 공격 이동이 선택한 Mass 타깃은 Actor 포인터를 가질 수 없다.
		// 클릭이 사거리 안이면 클릭점 기준, 밖이면 공격자 기준으로 하나를 고른 뒤
		// ResolveAndApplyBestTarget이 그 Entity ID에만 피해를 적용한다.
		const float AllowedRange = FMath::Max(0.0f, Context.Definition->AttackRange) + 15.0f;
		const bool bClickIsInAttackRange = FVector::DistSquared2D(Start, Context.TargetLocation)
			<= FMath::Square(AllowedRange);
		FASCombatTargetQuery Query;
		Query.SearchOrigin = Start;
		Query.SearchRadius = AllowedRange;
		Query.PriorityLocation = Context.TargetLocation;
		Query.Instigator = Context.Avatar;
		Query.Domain = EASCombatTargetDomain::MassOnly;
		Query.SortPolicy = bClickIsInAttackRange
			? EASCombatTargetSortPolicy::NearestToPriorityLocation
			: EASCombatTargetSortPolicy::NearestToSearchOrigin;
		Query.MaxTargets = 1;
		const FASCombatApplyResult ApplyResult = CombatTargets->ResolveAndApplyBestTarget(Query, DamageSpecs);
		if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
		{
			Character->MulticastDrawWeaponAttackDebug(Start, Context.TargetLocation, TraceRadius, false,
				ApplyResult.HasAnyHit());
		}
		return;
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
		if (UASCombatTargetSubsystem::IsValidActorTarget(Target, Context.Avatar, false)
			&& (!FirstTarget || Hit.Time < FirstHitTime))
		{
			FirstTarget = Target;
			FirstHitTime = Hit.Time;
		}
	}

	FASCombatApplyResult ApplyResult;
	if (FirstTarget)
	{
		FASCombatTargetQuery Query;
		Query.Instigator = Context.Avatar;
		if (Profile->TargetMode == EASAttackTargetMode::Area)
		{
			const FVector ImpactCenter = Start + (End - Start) * FirstHitTime;
			Query.SearchOrigin = ImpactCenter;
			Query.SearchRadius = FMath::Max(0.0f, Profile->AreaRadius);
			ApplyResult = CombatTargets->ResolveAndApply(Query, DamageSpecs);
			if (!ApplyResult.ActorTargets.Contains(FirstTarget))
			{
				FASCombatTargetQuery DirectQuery;
				DirectQuery.SearchOrigin = FirstTarget->GetActorLocation();
				DirectQuery.SearchRadius = 1.0f;
				DirectQuery.Instigator = Context.Avatar;
				DirectQuery.Domain = EASCombatTargetDomain::ActorOnly;
				DirectQuery.ExplicitActor = FirstTarget;
				DirectQuery.MaxTargets = 1;
				ApplyResult.ActorTargets.Append(CombatTargets->ResolveAndApply(DirectQuery, DamageSpecs).ActorTargets);
			}
		}
		else
		{
			Query.SearchOrigin = FirstTarget->GetActorLocation();
			Query.SearchRadius = 1.0f;
			Query.Domain = EASCombatTargetDomain::ActorOnly;
			Query.ExplicitActor = FirstTarget;
			Query.MaxTargets = 1;
			ApplyResult = CombatTargets->ResolveAndApply(Query, DamageSpecs);
		}
	}
	else
	{
		FASCombatTargetHitSet HitTargets;
		FASCombatTargetQuery Query;
		Query.Shape = EASCombatTargetShape::Segment;
		Query.SearchOrigin = Start;
		Query.EndLocation = End;
		Query.SearchRadius = TraceRadius;
		Query.Instigator = Context.Avatar;
		Query.Domain = EASCombatTargetDomain::MassOnly;
		Query.MaxTargets = 1;
		ApplyResult = CombatTargets->ResolveAndApply(Query, DamageSpecs, &HitTargets);
	}
	if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
	{
		const FVector DebugEnd = FirstTarget ? Start + (End - Start) * FirstHitTime : End;
		Character->MulticastDrawWeaponAttackDebug(Start, DebugEnd, TraceRadius, false,
			ApplyResult.HasAnyHit());
	}
}
