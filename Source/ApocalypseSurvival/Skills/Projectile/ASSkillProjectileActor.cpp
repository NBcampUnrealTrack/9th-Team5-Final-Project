#include "Skills/Projectile/ASSkillProjectileActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Skills/ASSkillAbility.h"
#include "TimerManager.h"

AASSkillProjectileActor::AASSkillProjectileActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	// 적(Pawn)은 통과하며 오버랩, 벽(WorldStatic)에는 막힘.
	// 실제 설정은 InitProjectile에서도 한 번 더 적용한다. BP 자식의 기본값이 이를 덮어쓸 수 있기 때문이다.
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(20.0f);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AASSkillProjectileActor::HandleOverlap);
	RootComponent = CollisionComponent;
	ConfigureProjectileCollision();

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
	TArray<FGameplayEffectSpecHandle> Specs;
	if (InDamageSpec.IsValid())
	{
		Specs.Add(InDamageSpec);
	}
	InitProjectile(Specs, InInstigatorAvatar, Direction, Speed, MaxRange, InPierceCount, bInCanHitPlayers);
}

void AASSkillProjectileActor::InitProjectile(
	const TArray<FGameplayEffectSpecHandle>& InDamageSpecs,
	AActor* InInstigatorAvatar,
	const FVector& Direction,
	float Speed,
	float MaxRange,
	int32 InPierceCount,
	bool bInCanHitPlayers)
{
	DamageSpecs = InDamageSpecs;
	InstigatorAvatar = InInstigatorAvatar;
	PierceCount = InPierceCount;
	bCanHitPlayers = bInCanHitPlayers;
	ConfigureProjectileCollision();

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

void AASSkillProjectileActor::ConfigureProjectileCollision()
{
	if (!CollisionComponent)
	{
		return;
	}

	// 피격 판정 Sphere만 Query 충돌을 사용한다. Pawn은 반드시 Overlap이므로 물리적으로 밀어내지 않는다.
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCanEverAffectNavigation(false);

	// BP에 붙인 StaticMesh/SkeletalMesh 등의 시각 컴포넌트가 Block 상태면 Sphere 설정과 무관하게
	// 캐릭터를 밀어낼 수 있다. 시각 컴포넌트는 충돌·오버랩·NavMesh에 전혀 참여시키지 않는다.
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent != CollisionComponent)
		{
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PrimitiveComponent->SetGenerateOverlapEvents(false);
			PrimitiveComponent->SetCanEverAffectNavigation(false);
		}
	}
}

void AASSkillProjectileActor::BeginPlay()
{
	Super::BeginPlay();

	// Deferred Spawn의 InitProjectile 뒤에는 FinishSpawning 과정에서 BP Construction Script가 실행된다.
	// 그 스크립트가 Mesh/Collision 기본값을 다시 적용할 수 있으므로, 실제 비행 직전에 다시 강제한다.
	ConfigureProjectileCollision();

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
	for (const FGameplayEffectSpecHandle& DamageSpec : DamageSpecs)
	{
		UASSkillAbility::ApplyDamageSpecToTarget(DamageSpec, Target);
	}
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
