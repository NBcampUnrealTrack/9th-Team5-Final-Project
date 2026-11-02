#include "Items/Combat/ASShotgunAttackSequence.h"

#include "AbilitySystemComponent.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackExecution.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "TimerManager.h"
#include "Components/SphereComponent.h"

void AASShotgunPelletProjectile::InitShotgunPellet(
	AASShotgunAttackSequence* InSequence, const int32 InPelletIndex)
{
	Sequence = InSequence;
	PelletIndex = InPelletIndex;
	bResolveSegmentHits = false;
}

void AASShotgunPelletProjectile::BeginPlay()
{
	Super::BeginPlay();
	PreviousPelletLocation = GetActorLocation();
}

void AASShotgunPelletProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bPelletResolved || IsActorBeingDestroyed())
	{
		return;
	}
	ResolvePelletHit(GetActorLocation());
}

void AASShotgunPelletProjectile::ResolvePelletHit(const FVector& CurrentLocation)
{
	if (bPelletResolved)
	{
		return;
	}

	UASCombatTargetSubsystem* CombatTargets = GetWorld()
		? GetWorld()->GetSubsystem<UASCombatTargetSubsystem>()
		: nullptr;
	if (!CombatTargets)
	{
		PreviousPelletLocation = CurrentLocation;
		return;
	}
	if (CombatTargets->DeferUntilMassIdle(DeferredPelletHitTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			DeferredPelletHitTimer.Invalidate();
			ResolvePelletHit(GetActorLocation());
		})))
	{
		return;
	}

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Segment;
	Query.SearchOrigin = PreviousPelletLocation;
	Query.EndLocation = CurrentLocation;
	Query.SearchRadius = CollisionComponent ? CollisionComponent->GetScaledSphereRadius() : 20.0f;
	Query.Instigator = InstigatorAvatar.Get();
	Query.bCanHitPlayers = CanHitPlayers();
	Query.MaxTargets = 1;

	FASCombatTargetRef Target;
	if (!CombatTargets->FindBestTarget(Query, Target))
	{
		PreviousPelletLocation = CurrentLocation;
		return;
	}

	FASCombatImpactContext ResolvedImpact = ImpactContext;
	ResolvedImpact.HitLocation = CurrentLocation;
	if (AASShotgunAttackSequence* Shotgun = Sequence.Get())
	{
		ResolvedImpact = Shotgun->BuildImpactContextForPotentialHit(Target, ResolvedImpact);
	}

	const FASCombatApplyResult Result = CombatTargets->ApplyEffectSpecsToTarget(
		Target, DamageSpecs, InstigatorAvatar.Get(), ResolvedImpact);
	if (Result.HasAnyHit())
	{
		if (AASShotgunAttackSequence* Shotgun = Sequence.Get())
		{
			Shotgun->NotifyPelletHit(Target, ResolvedImpact);
		}
	}

	bPelletResolved = true;
	EndProjectile();
}

void AASShotgunPelletProjectile::OnProjectileFinishedNative()
{
	GetWorldTimerManager().ClearTimer(DeferredPelletHitTimer);
	NotifyFinished();
	Super::OnProjectileFinishedNative();
}

void AASShotgunPelletProjectile::NotifyFinished()
{
	if (bFinishNotified)
	{
		return;
	}
	bFinishNotified = true;
	if (AASShotgunAttackSequence* Shotgun = Sequence.Get())
	{
		Shotgun->NotifyPelletFinished();
	}
}

AASShotgunAttackSequence::AASShotgunAttackSequence()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorEnableCollision(false);
}

void AASShotgunAttackSequence::InitShotgun(
	const FASWeaponAttackExecutionContext& Context, const UASShotgunAttackProfile* InProfile)
{
	Avatar = Context.Avatar;
	AbilitySystem = Context.AbilitySystem;
	Weapon = Context.Weapon;
	Profile = const_cast<UASShotgunAttackProfile*>(InProfile);
	AttackModeValue = static_cast<uint8>(Context.AttackMode);
	AimDirection = Context.AimDirection;
	bInitialized = true;
}

void AASShotgunAttackSequence::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || !bInitialized || !Avatar.IsValid() || !AbilitySystem.IsValid()
		|| !Weapon.IsValid() || !Profile.IsValid())
	{
		Destroy();
		return;
	}
	SpawnPellets();
}

FVector AASShotgunAttackSequence::ResolveBaseDirection() const
{
	FVector Direction = AimDirection.GetSafeNormal2D();
	if (Direction.IsNearlyZero() && Avatar.IsValid())
	{
		Direction = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}
	return Direction;
}

FVector AASShotgunAttackSequence::ResolveSpawnLocation(const FVector& Direction) const
{
	const APawn* CurrentAvatar = Avatar.Get();
	const UASShotgunAttackProfile* CurrentProfile = Profile.Get();
	if (!CurrentAvatar || !CurrentProfile)
	{
		return FVector::ZeroVector;
	}

	FVector SpawnLocation = CurrentAvatar->GetActorLocation()
		+ Direction * CurrentProfile->ProjectileSpawnForwardOffset
		+ FVector(0.0f, 0.0f, CurrentProfile->ProjectileSpawnHeightOffset);
	if (!CurrentProfile->ProjectileSpawnSocketName.IsNone())
	{
		if (const ACharacter* Character = Cast<ACharacter>(CurrentAvatar))
		{
			if (const USkeletalMeshComponent* Mesh = Character->GetMesh();
				Mesh && Mesh->DoesSocketExist(CurrentProfile->ProjectileSpawnSocketName))
			{
				SpawnLocation = Mesh->GetSocketLocation(CurrentProfile->ProjectileSpawnSocketName);
			}
		}
	}
	return SpawnLocation;
}

FASCombatImpactContext AASShotgunAttackSequence::MakeBaseImpactContext(
	const FVector& SpawnLocation, const FVector& Direction, const int32 PelletIndex) const
{
	FASCombatImpactContext Result;
	Result.Preset = EASImpactPreset::None;
	Result.AttackInstanceId = static_cast<int32>((GetUniqueID() & 0x0fffffff) * 16 + PelletIndex);
	Result.HitLocation = SpawnLocation;
	Result.HitDirection = Direction;
	return Result;
}

void AASShotgunAttackSequence::SpawnPellets()
{
	APawn* CurrentAvatar = Avatar.Get();
	UASShotgunAttackProfile* CurrentProfile = Profile.Get();
	UASItemInstance* CurrentWeapon = Weapon.Get();
	UWorld* World = GetWorld();
	if (!CurrentAvatar || !CurrentProfile || !CurrentWeapon || !World)
	{
		bFinishedSpawning = true;
		TryFinishSequence();
		return;
	}
	const UASItemDefinition* Definition = CurrentWeapon->GetItemDefinition();
	const FVector BaseDirection = ResolveBaseDirection();
	if (!Definition || BaseDirection.IsNearlyZero())
	{
		bFinishedSpawning = true;
		TryFinishSequence();
		return;
	}

	const int32 PelletCount = FMath::Max(1, CurrentProfile->PelletCount);
	const float HalfSpread = FMath::Clamp(CurrentProfile->SpreadAngleDegrees, 1.0f, 180.0f) * 0.5f;
	const float AttackRange = static_cast<EASPrimaryAttackMode>(AttackModeValue) == EASPrimaryAttackMode::Directional
		? FMath::Max(0.0f, Definition->AttackRange)
		: AASPlayerCharacter::ResolveAttackRange(CurrentAvatar, Definition);

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		const float Alpha = PelletCount == 1 ? 0.5f : static_cast<float>(PelletIndex) / (PelletCount - 1);
		const float YawOffset = FMath::Lerp(-HalfSpread, HalfSpread, Alpha);
		const FVector Direction = BaseDirection.RotateAngleAxis(YawOffset, FVector::UpVector).GetSafeNormal2D();
		const FVector SpawnLocation = ResolveSpawnLocation(Direction);
		const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
		UClass* ProjectileClass = CurrentProfile->PelletProjectileClass.Get()
			? CurrentProfile->PelletProjectileClass.Get()
			: AASShotgunPelletProjectile::StaticClass();
		AASShotgunPelletProjectile* Pellet = World->SpawnActorDeferred<AASShotgunPelletProjectile>(
			ProjectileClass, SpawnTransform, CurrentAvatar, CurrentAvatar,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Pellet)
		{
			continue;
		}

		Pellet->InitShotgunPellet(this, PelletIndex);
		Pellet->InitProjectile(
			UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(CurrentWeapon, AbilitySystem.Get()),
			CurrentAvatar, Direction, CurrentProfile->ProjectileSpeed, AttackRange,
			0, false, MakeBaseImpactContext(SpawnLocation, Direction, PelletIndex));
		++ActivePelletCount;
		Pellet->FinishSpawning(SpawnTransform);
	}

	bFinishedSpawning = true;
	TryFinishSequence();
}

bool AASShotgunAttackSequence::HasAppliedKnockback(const FASCombatTargetRef& Target) const
{
	if (AActor* Actor = Target.GetActor())
	{
		return KnockbackActors.Contains(TWeakObjectPtr<AActor>(Actor));
	}
	return Target.IsMass() && KnockbackMassTargets.Contains(Target.GetMassTargetId());
}

FASCombatImpactContext AASShotgunAttackSequence::BuildImpactContextForPotentialHit(
	const FASCombatTargetRef& Target, const FASCombatImpactContext& BaseContext) const
{
	FASCombatImpactContext Result = BaseContext;
	const UASShotgunAttackProfile* CurrentProfile = Profile.Get();
	if (!CurrentProfile || HasAppliedKnockback(Target))
	{
		Result.Preset = EASImpactPreset::None;
		Result.KnockbackDistance = 0.0f;
		return Result;
	}

	Result.Preset = CurrentProfile->KnockbackImpactPreset;
	// 실제 위치 이동은 다른 Pellet이 같은 프레임에 명중할 시간을 준 뒤 Sequence가 한 번만 처리한다.
	Result.KnockbackDistance = 0.0f;
	return Result;
}

void AASShotgunAttackSequence::NotifyPelletHit(
	const FASCombatTargetRef& Target, const FASCombatImpactContext& AppliedImpactContext)
{
	const bool bAlreadyApplied = HasAppliedKnockback(Target);
	if (AActor* Actor = Target.GetActor())
	{
		KnockbackActors.Add(TWeakObjectPtr<AActor>(Actor));
	}
	else if (Target.IsMass())
	{
		KnockbackMassTargets.Add(Target.GetMassTargetId());
	}

	const UASShotgunAttackProfile* CurrentProfile = Profile.Get();
	if (bAlreadyApplied || !CurrentProfile || CurrentProfile->KnockbackDistance <= 0.0f)
	{
		return;
	}
	if (bKnockbackCommitted)
	{
		if (UASCombatTargetSubsystem* CombatTargets = GetWorld()
			? GetWorld()->GetSubsystem<UASCombatTargetSubsystem>() : nullptr)
		{
			FASCombatImpactContext KnockbackContext = AppliedImpactContext;
			KnockbackContext.KnockbackDistance = CurrentProfile->KnockbackDistance;
			CombatTargets->ApplyImpactKnockbackToTarget(Target, KnockbackContext);
		}
		return;
	}

	FPendingKnockback& Pending = PendingKnockbacks.AddDefaulted_GetRef();
	Pending.Target = Target;
	Pending.ImpactContext = AppliedImpactContext;
	Pending.ImpactContext.KnockbackDistance = CurrentProfile->KnockbackDistance;
	if (!bKnockbackPending && !bKnockbackCommitted)
	{
		bKnockbackPending = true;
		GetWorldTimerManager().SetTimer(KnockbackTimerHandle, this, &ThisClass::CommitKnockback, 0.05f, false);
	}
}

void AASShotgunAttackSequence::CommitKnockback()
{
	bKnockbackPending = false;
	bKnockbackCommitted = true;
	if (UASCombatTargetSubsystem* CombatTargets = GetWorld()
		? GetWorld()->GetSubsystem<UASCombatTargetSubsystem>() : nullptr)
	{
		for (const FPendingKnockback& Pending : PendingKnockbacks)
		{
			CombatTargets->ApplyImpactKnockbackToTarget(Pending.Target, Pending.ImpactContext);
		}
	}
	PendingKnockbacks.Reset();
	TryFinishSequence();
}

void AASShotgunAttackSequence::NotifyPelletFinished()
{
	ActivePelletCount = FMath::Max(0, ActivePelletCount - 1);
	TryFinishSequence();
}

void AASShotgunAttackSequence::TryFinishSequence()
{
	if (bFinishedSpawning && ActivePelletCount == 0 && !bKnockbackPending)
	{
		Destroy();
	}
}
