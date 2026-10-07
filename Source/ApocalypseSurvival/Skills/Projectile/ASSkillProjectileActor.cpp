#include "Skills/Projectile/ASSkillProjectileActor.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Skills/ASSkillAbility.h"
#include "TimerManager.h"

AASSkillProjectileActor::AASSkillProjectileActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	//적(Pawn)은 통과하며 오버랩, 벽(WorldStatic)에는 막힘
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(20.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AASSkillProjectileActor::HandleOverlap);
	RootComponent = CollisionComponent;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 2000.0f;
	ProjectileMovement->MaxSpeed = 2000.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AASSkillProjectileActor::HandleStop);
}

void AASSkillProjectileActor::InitProjectile(
	const FGameplayEffectSpecHandle& InDamageSpec,
	AActor* InInstigatorAvatar,
	const FVector& Direction,
	float Speed,
	float MaxRange,
	int32 InPierceCount,
	bool bInCanHitPlayers)
{
	DamageSpec = InDamageSpec;
	InstigatorAvatar = InInstigatorAvatar;
	PierceCount = InPierceCount;
	bCanHitPlayers = bInCanHitPlayers;

	Speed = FMath::Max(1.0f, Speed);
	FlightTime = FMath::Max(0.05f, MaxRange / Speed);

	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = Direction.GetSafeNormal() * Speed;

	if (InInstigatorAvatar)
	{
		CollisionComponent->IgnoreActorWhenMoving(InInstigatorAvatar, true);
	}
}

void AASSkillProjectileActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		//최대 거리에 도달하면 사라짐
		GetWorldTimerManager().SetTimer(RangeTimerHandle, this, &AASSkillProjectileActor::EndProjectile, FlightTime, false);
	}
}

void AASSkillProjectileActor::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bEnded)
	{
		return;
	}

	if (!UASSkillAbility::IsValidSkillTarget(OtherActor, InstigatorAvatar.Get(), bCanHitPlayers) || HitActors.Contains(OtherActor))
	{
		return;
	}

	HitActors.Add(OtherActor);
	OnHitTarget(OtherActor);

	if (PierceCount >= 0 && HitActors.Num() > PierceCount)
	{
		EndProjectile();
	}
}

void AASSkillProjectileActor::HandleStop(const FHitResult& ImpactResult)
{
	if (HasAuthority())
	{
		EndProjectile();
	}
}

void AASSkillProjectileActor::OnHitTarget_Implementation(AActor* Target)
{
	UASSkillAbility::ApplyDamageSpecToTarget(DamageSpec, Target);
}

void AASSkillProjectileActor::OnProjectileEnd_Implementation()
{
}

void AASSkillProjectileActor::EndProjectile()
{
	if (bEnded)
	{
		return;
	}

	bEnded = true;
	GetWorldTimerManager().ClearTimer(RangeTimerHandle);

	OnProjectileEnd();
	Destroy();
}
