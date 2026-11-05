#include "Items/Combat/ASWeaponAttackExecution.h"

#include "AbilitySystemComponent.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Core/Combat/ASCombatDiagnostics.h"
#include "Core/Combat/ASCombatImpactSettings.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "GameFramework/Character.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/Combat/ASWeaponProjectile.h"
#include "Items/Combat/ASRifleBurstSequence.h"
#include "Items/Combat/ASShotgunAttackSequence.h"
#include "Items/Combat/ASSniperPiercingProjectile.h"
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

	FASCombatImpactContext MakeImpactContext(const FASWeaponAttackExecutionContext& Context,
		const FVector& HitLocation, const FVector& HitDirection)
	{
		FASCombatImpactContext Result;
		Result.Preset = Context.Profile ? Context.Profile->ImpactPreset : EASImpactPreset::None;
		Result.HitLocation = HitLocation;
		Result.HitDirection = HitDirection.GetSafeNormal();
		return Result;
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
		UClass* ProjectileClass = Profile.DirectionalProjectileClass.Get()
			? Profile.DirectionalProjectileClass.Get()
			: (Profile.ProjectileClass.Get()
				? Profile.ProjectileClass.Get()
				: AASSkillProjectileActor::StaticClass());
		AASSkillProjectileActor* Projectile = World->SpawnActorDeferred<AASSkillProjectileActor>(
			ProjectileClass, SpawnTransform, Context.Avatar, Context.Avatar,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile)
		{
			return;
		}

		const TArray<FGameplayEffectSpecHandle> DamageSpecs = MakeDamageSpecs(Context);
		const FASCombatImpactContext ImpactContext = MakeImpactContext(Context, SpawnLocation, Direction);
		const float ProjectileRange = Context.AttackMode == EASPrimaryAttackMode::Directional
			? FMath::Max(0.0f, Context.Definition->AttackRange)
			: AASPlayerCharacter::ResolveAttackRange(Context.Avatar, Context.Definition);
		Projectile->InitProjectile(DamageSpecs, Context.Avatar,
			Direction, Profile.ProjectileSpeed, ProjectileRange, Profile.ProjectilePierceCount, false, ImpactContext);
		Projectile->FinishSpawning(SpawnTransform);
	}
	
	void SpawnHomingProjectile(const FASWeaponAttackExecutionContext& Context, const UASRangedAttackProfile& Profile,
	const FASCombatTargetRef& Target)
	{
		UWorld* World = Context.Avatar ? Context.Avatar->GetWorld() : nullptr;
		if (!World || !Profile.HomingProjectileClass.Get())
		{
			return;
		}

		FVector Direction = (Target.GetLocation() - Context.Avatar->GetActorLocation()).GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = Context.Avatar->GetActorForwardVector().GetSafeNormal2D();
		}

		// 발사 위치: 캐릭터 앞 + 높이 (소켓이 지정돼 있으면 소켓 위치)
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
		AASWeaponProjectile* Projectile = World->SpawnActorDeferred<AASWeaponProjectile>(
			Profile.HomingProjectileClass.Get(), SpawnTransform, Context.Avatar, Context.Avatar,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile)
		{
			return;
		}

		const FASCombatImpactContext ImpactContext = MakeImpactContext(Context, SpawnLocation, Direction);
		Projectile->InitHoming(MakeDamageSpecs(Context), Context.Avatar, Target, Profile.HomingProjectileSpeed, ImpactContext);
		Projectile->FinishSpawning(SpawnTransform);
	}
}

bool UASMeleeAttackExecution::CanPayCost(const FASWeaponAttackExecutionContext& Context) const
{
	const UASMeleeAttackProfile* Profile = Cast<UASMeleeAttackProfile>(Context.Profile);
	return Profile
		&& Context.Definition && Context.Definition->WeaponType == EASWeaponType::Melee
		&& IsValid(Context.AbilitySystem)
		&& Profile->StaminaCostEffect.Get()
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
		Profile->StaminaCostEffect.Get(), 1.0f, EffectContext);
	if (!CostSpec.IsValid()) return false;
	const FGameplayTag StaminaCostTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.StaminaCost")), false);
	if (!StaminaCostTag.IsValid()) return false;
	CostSpec.Data->SetSetByCallerMagnitude(StaminaCostTag, -ASWeaponAttackExecutionPrivate::GetStaminaCost(Context));
	Context.AbilitySystem->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());

	if (ASWeaponAttackExecutionPrivate::GetStaminaCost(Context) > 0.0f
		&& Profile->StaminaRegenDelayEffect.Get())
	{
		const FGameplayEffectSpecHandle DelaySpec = Context.AbilitySystem->MakeOutgoingSpec(
			Profile->StaminaRegenDelayEffect.Get(), 1.0f, EffectContext);
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

	UASCombatTargetSubsystem* CombatTargets =
		Context.Avatar->GetWorld()->GetSubsystem<UASCombatTargetSubsystem>();
	if (!CombatTargets)
	{
		return;
	}

	// 캐릭터 위치 기준 앞쪽 부채꼴. 반경 = 무기 사거리, 각도 = 프로필
	const float ResolvedAttackRange = Context.AttackMode == EASPrimaryAttackMode::Directional
		? FMath::Max(0.0f, Context.Definition->AttackRange)
		: AASPlayerCharacter::ResolveAttackRange(Context.Avatar, Context.Definition);
	const float AttackRange = FMath::Max(1.0f, ResolvedAttackRange);
	FVector Direction = Context.AimDirection.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Context.Avatar->GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector Origin = Context.Avatar->GetActorLocation();

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Cone;
	Query.SearchOrigin = Origin;
	Query.Direction = Direction;
	Query.SearchRadius = AttackRange;
	Query.AngleDegrees = Profile->HitAngle;
	Query.Instigator = Context.Avatar;

	// 부채꼴 안의 적 전부 (일반 몬스터 + Mass 몬스터)
	const TArray<FGameplayEffectSpecHandle> DamageSpecs = ASWeaponAttackExecutionPrivate::MakeDamageSpecs(Context);
	const FASCombatImpactContext ImpactContext = ASWeaponAttackExecutionPrivate::MakeImpactContext(Context, Origin, Direction);
	const FASCombatApplyResult ApplyResult = CombatTargets->ResolveAndApply(Query, DamageSpecs, ImpactContext);

	if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
	{
		Character->MulticastDrawMeleeConeDebug(Origin, Direction, AttackRange, Profile->HitAngle, ApplyResult.HasAnyHit());
		if (ApplyResult.HasAnyHit())
		{
			const UASCombatImpactSettings* ImpactSettings = GetDefault<UASCombatImpactSettings>();
			const float HitStopDuration = ImpactSettings
				? ImpactSettings->GetMeleeHitStopDuration(ImpactContext.Preset)
				: 0.0f;
			Character->MulticastPlayMeleeHitStop(HitStopDuration);
		}
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
	const bool bUseDirectionalShot = Context.AttackMode == EASPrimaryAttackMode::Directional
		|| !Profile->bUseAttackMoveTargeting;
	if (bUseDirectionalShot)
	{
		ASWeaponAttackExecutionPrivate::SpawnDirectionalProjectile(Context, *Profile);
		return;
	}
	const TArray<FGameplayEffectSpecHandle> DamageSpecs = ASWeaponAttackExecutionPrivate::MakeDamageSpecs(Context);
	FASCombatImpactContext ImpactContext = ASWeaponAttackExecutionPrivate::MakeImpactContext(
		Context, Start, Context.AimDirection);
	UASCombatTargetSubsystem* CombatTargets = World->GetSubsystem<UASCombatTargetSubsystem>();
	if (!CombatTargets)
	{
		return;
	}

		if (Profile->TargetMode == EASAttackTargetMode::Single)
	{
		const float AllowedRange = FMath::Max(0.0f, 	AASPlayerCharacter::ResolveAttackRange(Context.Avatar, Context.Definition)) + 15.0f;

		// 대상 찾기 쿼리: 클릭한 액터 몬스터가 사거리 안이면 그 액터, 아니면 클릭 지점/공격자 기준 Mass 한 마리
		FASCombatTargetQuery Query;
		Query.Instigator = Context.Avatar;
		Query.MaxTargets = 1;
		FVector DebugEnd = Context.TargetLocation;

		AActor* RequestedTarget = Context.RequestedTarget.Get();
		const bool bRequestedActorInRange = IsValid(RequestedTarget)
			&& UASCombatTargetSubsystem::IsValidActorTarget(RequestedTarget, Context.Avatar, false)
			&& FVector::DistSquared2D(Start, RequestedTarget->GetActorLocation()) <= FMath::Square(AllowedRange);
		const bool bClickIsInAttackRange = FVector::DistSquared2D(Start, Context.TargetLocation)
			<= FMath::Square(AllowedRange);

		if (bRequestedActorInRange)
		{
			Query.SearchOrigin = RequestedTarget->GetActorLocation();
			Query.SearchRadius = 1.0f;
			Query.Domain = EASCombatTargetDomain::ActorOnly;
			Query.ExplicitActor = RequestedTarget;
			DebugEnd = RequestedTarget->GetActorLocation();
		}
		else
		{
			// 공격 이동이 선택한 Mass 타깃은 Actor 포인터를 가질 수 없다.
			// 클릭이 사거리 안이면 클릭점 기준, 밖이면 공격자 기준으로 하나를 고른다.
			Query.SearchOrigin = Start;
			Query.SearchRadius = AllowedRange;
			Query.PriorityLocation = Context.TargetLocation;
			Query.Domain = EASCombatTargetDomain::MassOnly;
			Query.SortPolicy = bClickIsInAttackRange
				? EASCombatTargetSortPolicy::NearestToPriorityLocation
				: EASCombatTargetSortPolicy::NearestToSearchOrigin;
		}

		// 유도 투사체: 대상만 정하고, 피해는 투사체가 닿을 때 준다.
		if (Profile->HomingProjectileClass.Get())
		{
			FASCombatTargetRef TargetRef;
			bool bFoundTarget = false;
			const bool bHasLockedMassTarget = Context.RequestedMassTargetNetworkId != 0;
			if (bHasLockedMassTarget)
			{
				// T1에서 서버가 검증한 안정 ID만 해석한다. 누락·사망·사거리 이탈 시
				// 클릭 위치로 다른 Mass 몬스터를 재선택하지 않고 이번 공격을 취소한다.
				bFoundTarget = CombatTargets->GetMassTargetByNetworkId(
					Context.RequestedMassTargetNetworkId, TargetRef);
				if (bFoundTarget)
				{
					const float TargetAllowedRange = AllowedRange + TargetRef.GetHitRadius();
					if (FVector::DistSquared2D(Start, TargetRef.GetLocation())
						> FMath::Square(TargetAllowedRange))
					{
						TargetRef.Reset();
						bFoundTarget = false;
					}
				}
			}
			else
			{
				bFoundTarget = CombatTargets->FindBestTarget(Query, TargetRef);
			}
			if (ASCombatDiagnostics::IsEnabled())
			{
				ASCombatDiagnostics::Record(Context.Avatar, FString::Printf(TEXT("Attack.RangedTarget Found=%d Mass=%d Specs=%d"),
					bFoundTarget, TargetRef.IsMass(), DamageSpecs.Num()));
			}
#if !UE_BUILD_SHIPPING
			const TCHAR* TargetKind = TargetRef.IsMass() ? TEXT("Mass")
				: (TargetRef.IsActor() ? TEXT("Actor") : TEXT("None"));
			UE_LOG(LogTemp, Log,
				TEXT("[MassRangedTarget][T2Fire] Avatar=%s Click=(%.1f,%.1f,%.1f) Search=(%.1f,%.1f,%.1f) Priority=(%.1f,%.1f,%.1f) ClickInRange=%d LockedMass=%d Found=%d Kind=%s MassEntityId=%llu RequestedMassNetworkId=%u ResolvedMassNetworkId=%u Selected=(%.1f,%.1f,%.1f)"),
				*GetNameSafe(Context.Avatar),
				Context.TargetLocation.X, Context.TargetLocation.Y, Context.TargetLocation.Z,
				Query.SearchOrigin.X, Query.SearchOrigin.Y, Query.SearchOrigin.Z,
				Query.PriorityLocation.X, Query.PriorityLocation.Y, Query.PriorityLocation.Z,
				bClickIsInAttackRange, bHasLockedMassTarget, bFoundTarget, TargetKind,
				static_cast<unsigned long long>(TargetRef.GetMassTargetId()), Context.RequestedMassTargetNetworkId,
				TargetRef.GetMassNetworkId(),
				TargetRef.GetLocation().X, TargetRef.GetLocation().Y, TargetRef.GetLocation().Z);
#endif
			if (bFoundTarget)
			{
				ASWeaponAttackExecutionPrivate::SpawnHomingProjectile(Context, *Profile, TargetRef);
			}
			if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
			{
				Character->MulticastDrawWeaponAttackDebug(Start, bFoundTarget ? TargetRef.GetLocation() : DebugEnd,
					0.0f, false, bFoundTarget);
			}
			return;
		}

		// 기존 즉시 판정 (투사체 클래스가 없는 무기)
		const FASCombatApplyResult ApplyResult = CombatTargets->ResolveAndApplyBestTarget(Query, DamageSpecs, ImpactContext);
		if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
		{
			Character->MulticastDrawWeaponAttackDebug(Start, DebugEnd, TraceRadius, false, ApplyResult.HasAnyHit());
		}
		return;
	}

	const FVector End = Start + Context.AimDirection * FMath::Max(1.0f, 	AASPlayerCharacter::ResolveAttackRange(Context.Avatar, Context.Definition));
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
			ImpactContext.HitLocation = ImpactCenter;
			ApplyResult = CombatTargets->ResolveAndApply(Query, DamageSpecs, ImpactContext);
			if (!ApplyResult.ActorTargets.Contains(FirstTarget))
			{
				FASCombatTargetQuery DirectQuery;
				DirectQuery.SearchOrigin = FirstTarget->GetActorLocation();
				DirectQuery.SearchRadius = 1.0f;
				DirectQuery.Instigator = Context.Avatar;
				DirectQuery.Domain = EASCombatTargetDomain::ActorOnly;
				DirectQuery.ExplicitActor = FirstTarget;
				DirectQuery.MaxTargets = 1;
				ApplyResult.ActorTargets.Append(CombatTargets->ResolveAndApply(DirectQuery, DamageSpecs, ImpactContext).ActorTargets);
			}
		}
		else
		{
			Query.SearchOrigin = FirstTarget->GetActorLocation();
			Query.SearchRadius = 1.0f;
			Query.Domain = EASCombatTargetDomain::ActorOnly;
			Query.ExplicitActor = FirstTarget;
			Query.MaxTargets = 1;
			ImpactContext.HitLocation = FirstTarget->GetActorLocation();
			ApplyResult = CombatTargets->ResolveAndApply(Query, DamageSpecs, ImpactContext);
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
		ApplyResult = CombatTargets->ResolveAndApply(Query, DamageSpecs, ImpactContext, &HitTargets);
	}
	if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
	{
		const FVector DebugEnd = FirstTarget ? Start + (End - Start) * FirstHitTime : End;
		Character->MulticastDrawWeaponAttackDebug(Start, DebugEnd, TraceRadius, false,
			ApplyResult.HasAnyHit());
	}
}

void UASRifleBurstAttackExecution::ExecuteHit(const FASWeaponAttackExecutionContext& Context) const
{
	UWorld* World = Context.Avatar ? Context.Avatar->GetWorld() : nullptr;
	const UASRifleBurstAttackProfile* RifleProfile = Cast<UASRifleBurstAttackProfile>(Context.Profile);
	if (!World || !Context.Avatar || !Context.Avatar->HasAuthority() || !RifleProfile)
	{
		return;
	}

	const FTransform SpawnTransform(Context.Avatar->GetActorTransform());
	AASRifleBurstSequence* Sequence = World->SpawnActorDeferred<AASRifleBurstSequence>(
		AASRifleBurstSequence::StaticClass(), SpawnTransform, Context.Avatar, Context.Avatar,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Sequence)
	{
		return;
	}
	Sequence->InitBurst(Context, RifleProfile);
	Sequence->FinishSpawning(SpawnTransform);
}

void UASShotgunAttackExecution::ExecuteHit(const FASWeaponAttackExecutionContext& Context) const
{
	UWorld* World = Context.Avatar ? Context.Avatar->GetWorld() : nullptr;
	const UASShotgunAttackProfile* ShotgunProfile = Cast<UASShotgunAttackProfile>(Context.Profile);
	if (!World || !Context.Avatar || !Context.Avatar->HasAuthority() || !ShotgunProfile)
	{
		return;
	}

	const FTransform SpawnTransform(Context.Avatar->GetActorTransform());
	AASShotgunAttackSequence* Sequence = World->SpawnActorDeferred<AASShotgunAttackSequence>(
		AASShotgunAttackSequence::StaticClass(), SpawnTransform, Context.Avatar, Context.Avatar,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Sequence)
	{
		return;
	}
	Sequence->InitShotgun(Context, ShotgunProfile);
	Sequence->FinishSpawning(SpawnTransform);
}

void UASSniperAttackExecution::ExecuteHit(const FASWeaponAttackExecutionContext& Context) const
{
	UWorld* World = Context.Avatar ? Context.Avatar->GetWorld() : nullptr;
	const UASSniperAttackProfile* SniperProfile = Cast<UASSniperAttackProfile>(Context.Profile);
	if (!World || !Context.Avatar || !Context.Avatar->HasAuthority() || !Context.Definition || !SniperProfile)
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
		+ Direction * SniperProfile->ProjectileSpawnForwardOffset
		+ FVector(0.0f, 0.0f, SniperProfile->ProjectileSpawnHeightOffset);
	if (!SniperProfile->ProjectileSpawnSocketName.IsNone())
	{
		if (const ACharacter* Character = Cast<ACharacter>(Context.Avatar))
		{
			if (const USkeletalMeshComponent* Mesh = Character->GetMesh();
				Mesh && Mesh->DoesSocketExist(SniperProfile->ProjectileSpawnSocketName))
			{
				SpawnLocation = Mesh->GetSocketLocation(SniperProfile->ProjectileSpawnSocketName);
			}
		}
	}

	const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
	UClass* ProjectileClass = SniperProfile->SniperProjectileClass.Get()
		? SniperProfile->SniperProjectileClass.Get()
		: AASSniperPiercingProjectile::StaticClass();
	AASSniperPiercingProjectile* Projectile = World->SpawnActorDeferred<AASSniperPiercingProjectile>(
		ProjectileClass, SpawnTransform, Context.Avatar, Context.Avatar,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return;
	}

	const float ProjectileRange = Context.AttackMode == EASPrimaryAttackMode::Directional
		? FMath::Max(0.0f, Context.Definition->AttackRange)
		: AASPlayerCharacter::ResolveAttackRange(Context.Avatar, Context.Definition);
	FASCombatImpactContext ImpactContext = ASWeaponAttackExecutionPrivate::MakeImpactContext(
		Context, SpawnLocation, Direction);
	ImpactContext.Preset = SniperProfile->ImpactPreset;
	Projectile->InitSniperPiercing(Context.Weapon, Context.AbilitySystem, SniperProfile->MaxPierceTargets,
		SniperProfile->PierceDamageMultipliers, SniperProfile->TraceRadius);
	Projectile->InitProjectile(TArray<FGameplayEffectSpecHandle>(), Context.Avatar, Direction,
		SniperProfile->ProjectileSpeed, ProjectileRange, -1, false, ImpactContext);
	Projectile->FinishSpawning(SpawnTransform);
}
