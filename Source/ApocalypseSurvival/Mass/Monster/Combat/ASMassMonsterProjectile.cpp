#include "Mass/Monster/Combat/ASMassMonsterProjectile.h"

#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Mass/Monster/Combat/ASMassMonsterDamageBridge.h"
#include "Mass/Monster/Subsystems/ASMonsterTargetSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterProjectile)

AASMassMonsterProjectile::AASMassMonsterProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(20.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCanEverAffectNavigation(false);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AASMassMonsterProjectile::HandleOverlap);
	RootComponent = CollisionComponent;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 1200.0f;
	ProjectileMovement->MaxSpeed = 1200.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AASMassMonsterProjectile::HandleStop);
}

void AASMassMonsterProjectile::InitializeProjectile(
	const float InDamage,
	const FVector& InSourceLocation,
	const FVector& Direction,
	float Speed,
	const float MaxRange)
{
	Damage = FMath::Max(0.0f, InDamage);
	SourceLocation = InSourceLocation;
	Speed = FMath::Max(1.0f, Speed);
	FlightTime = FMath::Max(0.05f, FMath::Max(0.0f, MaxRange) / Speed);
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = Direction.GetSafeNormal() * Speed;
}

void AASMassMonsterProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RangeTimerHandle, this,
			&AASMassMonsterProjectile::EndProjectile, FlightTime, false);
	}
}

void AASMassMonsterProjectile::HandleOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || bEnded)
	{
		return;
	}

	UASMonsterTargetSubsystem* TargetSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UASMonsterTargetSubsystem>()
		: nullptr;
	if (!TargetSubsystem || !TargetSubsystem->FindTargetForActor(OtherActor).IsValid())
	{
		return;
	}

	const ASMassMonsterDamage::EApplyResult Result =
		ASMassMonsterDamage::ApplyToActor(OtherActor, Damage, SourceLocation,
			SweepResult.ImpactPoint, this);
	if (Result != ASMassMonsterDamage::EApplyResult::InvalidTarget)
	{
		EndProjectile();
	}
}

void AASMassMonsterProjectile::HandleStop(const FHitResult& ImpactResult)
{
	if (HasAuthority())
	{
		EndProjectile();
	}
}

void AASMassMonsterProjectile::EndProjectile()
{
	if (bEnded)
	{
		return;
	}

	bEnded = true;
	GetWorldTimerManager().ClearTimer(RangeTimerHandle);
	Destroy();
}
