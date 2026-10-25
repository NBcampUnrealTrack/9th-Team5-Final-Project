#include "Items/Combat/ASWeaponProjectile.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"

AASWeaponProjectile::AASWeaponProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicateMovement(true);
	// 빠르게 움직이므로 위치를 자주 보낸다.
	SetNetUpdateFrequency(60.0f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AASWeaponProjectile::InitHoming(const TArray<FGameplayEffectSpecHandle>& InDamageSpecs, AActor* InInstigatorAvatar,
	const FASCombatTargetRef& InTarget, const float InSpeed)
{
	DamageSpecs = InDamageSpecs;
	InstigatorAvatar = InInstigatorAvatar;
	Target = InTarget;
	LastTargetLocation = InTarget.GetLocation();
	Speed = FMath::Max(1.0f, InSpeed);
	bInitialized = true;
#if !UE_BUILD_SHIPPING
	const TCHAR* TargetKind = InTarget.IsMass() ? TEXT("Mass")
		: (InTarget.IsActor() ? TEXT("Actor") : TEXT("None"));
	UE_LOG(LogTemp, Log,
		TEXT("[MassRangedTarget][T2Projectile] Projectile=%s Instigator=%s Kind=%s MassEntityId=%llu MassNetworkId=%u Target=(%.1f,%.1f,%.1f)"),
		*GetNameSafe(this), *GetNameSafe(InInstigatorAvatar),
		TargetKind,
		static_cast<unsigned long long>(InTarget.GetMassTargetId()),
		InTarget.GetMassNetworkId(),
		InTarget.GetLocation().X, InTarget.GetLocation().Y, InTarget.GetLocation().Z);
#endif
}

void AASWeaponProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 이동과 판정은 서버만 한다. 클라이언트는 복제된 위치를 받는다.
	if (HasAuthority() && bInitialized)
	{
		SetActorTickEnabled(true);
	}
}

void AASWeaponProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bImpacted)
	{
		return;
	}

	Elapsed += DeltaSeconds;
	if (Elapsed >= MaxLifetime)
	{
		Impact(false);
		return;
	}

	// 대상의 최신 위치로 갱신한다. 대상이 죽으면 Target 이 비워지고 마지막 위치로 날아간다.
	bool bTargetAlive = false;
	if (Target.IsValid())
	{
		if (UASCombatTargetSubsystem* CombatTargets = GetWorld()->GetSubsystem<UASCombatTargetSubsystem>())
		{
			FASCombatTargetQuery Query;
			Query.Instigator = InstigatorAvatar.Get();
			bTargetAlive = CombatTargets->RefreshTarget(Query, Target);
			if (bTargetAlive)
			{
				LastTargetLocation = Target.GetLocation();
			}
		}
	}

	// 높이는 유지하고 평면으로 따라간다 (탑다운).
	const FVector CurrentLocation = GetActorLocation();
	const FVector Goal(LastTargetLocation.X, LastTargetLocation.Y, CurrentLocation.Z);
	const FVector ToGoal = Goal - CurrentLocation;
	const float Distance = ToGoal.Size2D();
	const float Step = Speed * DeltaSeconds;
	const float HitDistance = (bTargetAlive ? Target.GetHitRadius() : 0.0f) + ImpactTolerance;

	if (Distance <= HitDistance + Step)
	{
		SetActorLocation(Goal);
		Impact(bTargetAlive);
		return;
	}

	const FVector Direction = ToGoal.GetSafeNormal2D();
	SetActorLocationAndRotation(CurrentLocation + Direction * Step, Direction.Rotation());
}

void AASWeaponProjectile::Impact(const bool bHasTarget)
{
	if (bImpacted)
	{
		return;
	}
	bImpacted = true;
	SetActorTickEnabled(false);

	bool bHit = false;
	UASCombatTargetSubsystem* CombatTargets = GetWorld()->GetSubsystem<UASCombatTargetSubsystem>();
	if (bHasTarget && CombatTargets)
	{
		// 유도 중 유지한 확정 대상에만 피해를 적용한다. 위치 기반 재검색은 하지 않는다.
		bHit = CombatTargets->ApplyEffectSpecsToTarget(
			Target, DamageSpecs, InstigatorAvatar.Get()).HasAnyHit();
	}

#if !UE_BUILD_SHIPPING
	const TCHAR* TargetKind = Target.IsMass() ? TEXT("Mass")
		: (Target.IsActor() ? TEXT("Actor") : TEXT("None"));
	UE_LOG(LogTemp, Log,
		TEXT("[MassRangedTarget][T3Impact] Projectile=%s Hit=%d Kind=%s MassEntityId=%llu MassNetworkId=%u Target=(%.1f,%.1f,%.1f) Impact=(%.1f,%.1f,%.1f)"),
		*GetNameSafe(this), bHit,
		TargetKind,
		static_cast<unsigned long long>(Target.GetMassTargetId()),
		Target.GetMassNetworkId(),
		Target.GetLocation().X, Target.GetLocation().Y, Target.GetLocation().Z,
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
#endif

	MulticastImpact(GetActorLocation(), bHit);

	// 명중 알림이 클라이언트에 도착할 시간을 두고 사라진다. 그동안은 숨긴다.
	SetActorHiddenInGame(true);
	SetLifeSpan(0.2f);
}

void AASWeaponProjectile::MulticastImpact_Implementation(const FVector ImpactLocation, const bool bHit)
{
	OnProjectileImpact(ImpactLocation, bHit);
}
