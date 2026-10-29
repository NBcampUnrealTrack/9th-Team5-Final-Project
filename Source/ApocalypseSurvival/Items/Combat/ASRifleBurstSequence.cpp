#include "Items/Combat/ASRifleBurstSequence.h"

#include "AbilitySystemComponent.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GAS/ASAbilityTags.h"
#include "GameFramework/Character.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackExecution.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "TimerManager.h"
#include "Components/SphereComponent.h"

void AASRifleBurstDirectionalProjectile::InitBurstShot(
	AASRifleBurstSequence* InSequence, const int32 InShotIndex)
{
	Sequence = InSequence;
	ShotIndex = InShotIndex;
	bResolveSegmentHits = false;
}

void AASRifleBurstDirectionalProjectile::BeginPlay()
{
	Super::BeginPlay();
	PreviousBurstLocation = GetActorLocation();
}

void AASRifleBurstDirectionalProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bBurstResolved || IsActorBeingDestroyed())
	{
		return;
	}
	ResolveBurstHit(GetActorLocation());
}

void AASRifleBurstDirectionalProjectile::ResolveBurstHit(const FVector& CurrentLocation)
{
	if (bBurstResolved)
	{
		return;
	}

	UASCombatTargetSubsystem* CombatTargets = GetWorld()
		? GetWorld()->GetSubsystem<UASCombatTargetSubsystem>()
		: nullptr;
	if (!CombatTargets)
	{
		PreviousBurstLocation = CurrentLocation;
		return;
	}
	if (CombatTargets->DeferUntilMassIdle(DeferredBurstHitTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			DeferredBurstHitTimer.Invalidate();
			ResolveBurstHit(GetActorLocation());
		})))
	{
		return;
	}

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Segment;
	Query.SearchOrigin = PreviousBurstLocation;
	Query.EndLocation = CurrentLocation;
	Query.SearchRadius = CollisionComponent ? CollisionComponent->GetScaledSphereRadius() : 20.0f;
	Query.Instigator = InstigatorAvatar.Get();
	Query.bCanHitPlayers = CanHitPlayers();
	Query.MaxTargets = 1;

	FASCombatTargetRef Target;
	if (!CombatTargets->FindBestTarget(Query, Target))
	{
		PreviousBurstLocation = CurrentLocation;
		return;
	}

	FASCombatImpactContext ResolvedImpact = ImpactContext;
	ResolvedImpact.HitLocation = CurrentLocation;
	if (AASRifleBurstSequence* Burst = Sequence.Get())
	{
		ResolvedImpact = Burst->BuildImpactContextForPotentialHit(Target, ResolvedImpact, ShotIndex);
	}

	const FASCombatApplyResult Result = CombatTargets->ApplyEffectSpecsToTarget(
		Target, DamageSpecs, InstigatorAvatar.Get(), ResolvedImpact);
	if (Result.HasAnyHit())
	{
		if (AASRifleBurstSequence* Burst = Sequence.Get())
		{
			Burst->NotifyProjectileHit(Target);
		}
	}

	bBurstResolved = true;
	EndProjectile();
}

void AASRifleBurstDirectionalProjectile::OnProjectileFinishedNative()
{
	GetWorldTimerManager().ClearTimer(DeferredBurstHitTimer);
	NotifyFinished();
	Super::OnProjectileFinishedNative();
}

void AASRifleBurstDirectionalProjectile::NotifyFinished()
{
	if (bFinishNotified)
	{
		return;
	}
	bFinishNotified = true;
	if (AASRifleBurstSequence* Burst = Sequence.Get())
	{
		Burst->NotifyProjectileFinished();
	}
}

void AASRifleBurstHomingProjectile::InitBurstShot(
	AASRifleBurstSequence* InSequence, const int32 InShotIndex, const FASCombatTargetRef& InLockedTarget)
{
	Sequence = InSequence;
	ShotIndex = InShotIndex;
	LockedTarget = InLockedTarget;
}

FASCombatImpactContext AASRifleBurstHomingProjectile::ResolveImpactContextForTarget(
	const FASCombatTargetRef& InTarget, const FASCombatImpactContext& BaseContext) const
{
	if (const AASRifleBurstSequence* Burst = Sequence.Get())
	{
		return Burst->BuildImpactContextForPotentialHit(InTarget, BaseContext, ShotIndex);
	}
	return BaseContext;
}

void AASRifleBurstHomingProjectile::OnHomingProjectileFinished(const bool bHit)
{
	if (bFinishNotified)
	{
		return;
	}
	bFinishNotified = true;
	if (AASRifleBurstSequence* Burst = Sequence.Get())
	{
		if (bHit)
		{
			Burst->NotifyProjectileHit(LockedTarget);
		}
		Burst->NotifyProjectileFinished();
	}
}

AASRifleBurstSequence::AASRifleBurstSequence()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorEnableCollision(false);
}

void AASRifleBurstSequence::InitBurst(
	const FASWeaponAttackExecutionContext& Context, const UASRifleBurstAttackProfile* InProfile)
{
	Avatar = Context.Avatar;
	AbilitySystem = Context.AbilitySystem;
	Inventory = Context.Inventory;
	Weapon = Context.Weapon;
	Profile = const_cast<UASRifleBurstAttackProfile*>(InProfile);
	RequestedTarget = Context.RequestedTarget;
	RequestedMassTargetNetworkId = Context.RequestedMassTargetNetworkId;
	AimDirection = Context.AimDirection;
	TargetLocation = Context.TargetLocation;
	AttackModeValue = static_cast<uint8>(Context.AttackMode);
	bInitialized = true;
}

void AASRifleBurstSequence::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || !bInitialized || !IsBurstStillValid())
	{
		Destroy();
		return;
	}
	FireNextShot();
}

bool AASRifleBurstSequence::IsBurstStillValid() const
{
	const APawn* CurrentAvatar = Avatar.Get();
	const UASInventoryComponent* CurrentInventory = Inventory.Get();
	const UASItemInstance* CurrentWeapon = Weapon.Get();
	const UASRifleBurstAttackProfile* CurrentProfile = Profile.Get();
	if (!HasAuthority() || !IsValid(CurrentAvatar) || !IsValid(CurrentInventory)
		|| !IsValid(CurrentWeapon) || !IsValid(CurrentProfile)
		|| CurrentInventory->GetEquippedItem() != CurrentWeapon
		|| CurrentWeapon->GetItemDefinition() == nullptr
		|| CurrentWeapon->GetItemDefinition()->AttackProfile != CurrentProfile)
	{
		return false;
	}
	if (const UAbilitySystemComponent* ASC = AbilitySystem.Get();
		!ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead))
	{
		return false;
	}
	if (const AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(CurrentAvatar))
	{
		return !Character->IsDead() && !Character->IsDowned();
	}
	return true;
}

FVector AASRifleBurstSequence::ResolveAimDirection() const
{
	FVector Direction = AimDirection.GetSafeNormal2D();
	if (Direction.IsNearlyZero() && Avatar.IsValid())
	{
		Direction = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}
	return Direction;
}

FVector AASRifleBurstSequence::ResolveSpawnLocation(const FVector& Direction) const
{
	const APawn* CurrentAvatar = Avatar.Get();
	const UASRifleBurstAttackProfile* CurrentProfile = Profile.Get();
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

FASCombatImpactContext AASRifleBurstSequence::MakeBaseImpactContext(
	const FVector& SpawnLocation, const FVector& Direction, const int32 ShotIndex) const
{
	FASCombatImpactContext Result;
	Result.Preset = Profile.IsValid() ? Profile->ImpactPreset : EASImpactPreset::Rapid;
	Result.AttackInstanceId = static_cast<int32>((GetUniqueID() & 0x0fffffff) * 4 + ShotIndex);
	Result.HitLocation = SpawnLocation;
	Result.HitDirection = Direction;
	return Result;
}

bool AASRifleBurstSequence::FindLockedTarget(FASCombatTargetRef& OutTarget) const
{
	const APawn* CurrentAvatar = Avatar.Get();
	const UASItemInstance* CurrentWeapon = Weapon.Get();
	UWorld* World = GetWorld();
	if (!CurrentAvatar || !CurrentWeapon || !World)
	{
		return false;
	}
	UASCombatTargetSubsystem* CombatTargets =
		World->GetSubsystem<UASCombatTargetSubsystem>();
	if (!CombatTargets)
	{
		return false;
	}

	if (RequestedMassTargetNetworkId != 0)
	{
		if (!CombatTargets->GetMassTargetByNetworkId(RequestedMassTargetNetworkId, OutTarget))
		{
			return false;
		}
	}
	else
	{
		AActor* ActorTarget = RequestedTarget.Get();
		if (!UASCombatTargetSubsystem::IsValidActorTarget(ActorTarget, CurrentAvatar, false))
		{
			return false;
		}
		FASCombatTargetQuery Query;
		Query.SearchOrigin = ActorTarget->GetActorLocation();
		Query.SearchRadius = 1.0f;
		Query.Instigator = CurrentAvatar;
		Query.Domain = EASCombatTargetDomain::ActorOnly;
		Query.ExplicitActor = ActorTarget;
		Query.MaxTargets = 1;
		if (!CombatTargets->FindBestTarget(Query, OutTarget))
		{
			return false;
		}
	}

	const UASItemDefinition* Definition = CurrentWeapon->GetItemDefinition();
	const float AllowedRange = Definition
		? AASPlayerCharacter::ResolveAttackRange(CurrentAvatar, Definition) + 15.0f + OutTarget.GetHitRadius()
		: 0.0f;
	return FVector::DistSquared2D(CurrentAvatar->GetActorLocation(), OutTarget.GetLocation())
		<= FMath::Square(FMath::Max(0.0f, AllowedRange));
}

bool AASRifleBurstSequence::SpawnDirectionalShot(const int32 ShotIndex)
{
	APawn* CurrentAvatar = Avatar.Get();
	UASRifleBurstAttackProfile* CurrentProfile = Profile.Get();
	UASItemInstance* CurrentWeapon = Weapon.Get();
	UWorld* World = GetWorld();
	if (!CurrentAvatar || !CurrentProfile || !CurrentWeapon || !World)
	{
		return false;
	}
	const UASItemDefinition* Definition = CurrentWeapon->GetItemDefinition();
	const FVector Direction = ResolveAimDirection();
	if (!Definition || Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector SpawnLocation = ResolveSpawnLocation(Direction);
	const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
	UClass* ProjectileClass = CurrentProfile->DirectionalProjectileClass
		&& CurrentProfile->DirectionalProjectileClass->IsChildOf(AASRifleBurstDirectionalProjectile::StaticClass())
		? CurrentProfile->DirectionalProjectileClass.Get()
		: AASRifleBurstDirectionalProjectile::StaticClass();
	AASRifleBurstDirectionalProjectile* Projectile = World->SpawnActorDeferred<AASRifleBurstDirectionalProjectile>(
		ProjectileClass, SpawnTransform, CurrentAvatar, CurrentAvatar,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return false;
	}

	Projectile->InitBurstShot(this, ShotIndex);
	Projectile->InitProjectile(
		UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(CurrentWeapon, AbilitySystem.Get()),
		CurrentAvatar, Direction, CurrentProfile->ProjectileSpeed, FMath::Max(0.0f, Definition->AttackRange),
		0, false, MakeBaseImpactContext(SpawnLocation, Direction, ShotIndex));
	++ActiveProjectileCount;
	Projectile->FinishSpawning(SpawnTransform);
	return true;
}

bool AASRifleBurstSequence::SpawnHomingShot(const int32 ShotIndex)
{
	APawn* CurrentAvatar = Avatar.Get();
	UASRifleBurstAttackProfile* CurrentProfile = Profile.Get();
	UASItemInstance* CurrentWeapon = Weapon.Get();
	UWorld* World = GetWorld();
	FASCombatTargetRef Target;
	if (!CurrentAvatar || !CurrentProfile || !CurrentWeapon || !World || !FindLockedTarget(Target))
	{
		return false;
	}

	FVector Direction = (Target.GetLocation() - CurrentAvatar->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = ResolveAimDirection();
	}
	const FVector SpawnLocation = ResolveSpawnLocation(Direction);
	const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
	UClass* ProjectileClass = CurrentProfile->HomingProjectileClass
		&& CurrentProfile->HomingProjectileClass->IsChildOf(AASRifleBurstHomingProjectile::StaticClass())
		? CurrentProfile->HomingProjectileClass.Get()
		: AASRifleBurstHomingProjectile::StaticClass();
	AASRifleBurstHomingProjectile* Projectile = World->SpawnActorDeferred<AASRifleBurstHomingProjectile>(
		ProjectileClass, SpawnTransform, CurrentAvatar, CurrentAvatar,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return false;
	}

	Projectile->InitBurstShot(this, ShotIndex, Target);
	Projectile->InitHoming(
		UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(CurrentWeapon, AbilitySystem.Get()),
		CurrentAvatar, Target, CurrentProfile->HomingProjectileSpeed,
		MakeBaseImpactContext(SpawnLocation, Direction, ShotIndex));
	++ActiveProjectileCount;
	Projectile->FinishSpawning(SpawnTransform);
	return true;
}

void AASRifleBurstSequence::FireNextShot()
{
	UASRifleBurstAttackProfile* CurrentProfile = Profile.Get();
	if (!CurrentProfile || !IsBurstStillValid())
	{
		StopSpawning();
		return;
	}

	const int32 ShotIndex = NextShotIndex++;
	const EASPrimaryAttackMode AttackMode = static_cast<EASPrimaryAttackMode>(AttackModeValue);
	bool bSpawned = false;
	if (AttackMode == EASPrimaryAttackMode::Directional)
	{
		bSpawned = SpawnDirectionalShot(ShotIndex);
	}
	else
	{
		bSpawned = SpawnHomingShot(ShotIndex);
	}
	if (!bSpawned)
	{
		StopSpawning();
		return;
	}

	if (NextShotIndex >= FMath::Max(2, CurrentProfile->BurstCount))
	{
		StopSpawning();
		return;
	}
	GetWorldTimerManager().SetTimer(
		ShotTimerHandle, this, &ThisClass::FireNextShot,
		FMath::Max(0.01f, CurrentProfile->BurstShotInterval), false);
}

void AASRifleBurstSequence::StopSpawning()
{
	if (bFinishedSpawning)
	{
		return;
	}
	bFinishedSpawning = true;
	GetWorldTimerManager().ClearTimer(ShotTimerHandle);
	TryFinishSequence();
}

FASCombatImpactContext AASRifleBurstSequence::BuildImpactContextForPotentialHit(
	const FASCombatTargetRef& Target, const FASCombatImpactContext& BaseContext, const int32 ShotIndex) const
{
	FASCombatImpactContext Result = BaseContext;
	const UASRifleBurstAttackProfile* CurrentProfile = Profile.Get();
	if (CurrentProfile
		&& ShotIndex == FMath::Max(2, CurrentProfile->BurstCount) - 1
		&& GetConfirmedHitCount(Target) + 1 >= FMath::Max(2, CurrentProfile->FocusedHitThreshold))
	{
		Result.Preset = CurrentProfile->FocusedImpactPreset;
	}
	return Result;
}

void AASRifleBurstSequence::NotifyProjectileHit(const FASCombatTargetRef& Target)
{
	if (AActor* Actor = Target.GetActor())
	{
		++ConfirmedActorHits.FindOrAdd(TWeakObjectPtr<AActor>(Actor));
	}
	else if (Target.IsMass())
	{
		++ConfirmedMassHits.FindOrAdd(Target.GetMassTargetId());
	}
}

int32 AASRifleBurstSequence::GetConfirmedHitCount(const FASCombatTargetRef& Target) const
{
	if (AActor* Actor = Target.GetActor())
	{
		if (const int32* Count = ConfirmedActorHits.Find(TWeakObjectPtr<AActor>(Actor)))
		{
			return *Count;
		}
	}
	else if (Target.IsMass())
	{
		if (const int32* Count = ConfirmedMassHits.Find(Target.GetMassTargetId()))
		{
			return *Count;
		}
	}
	return 0;
}

void AASRifleBurstSequence::NotifyProjectileFinished()
{
	ActiveProjectileCount = FMath::Max(0, ActiveProjectileCount - 1);
	TryFinishSequence();
}

void AASRifleBurstSequence::TryFinishSequence()
{
	if (bFinishedSpawning && ActiveProjectileCount == 0)
	{
		Destroy();
	}
}
