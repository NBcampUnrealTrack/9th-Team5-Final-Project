#include "Skills/Projectile/ASSkillProjectileActor.h"

#include "Skills/Debug/ASSkillDamageDebug.h"
#include "Skills/ASSkillFX.h"
#include "Core/Combat/ASCombatDiagnostics.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystem.h"
#include "TimerManager.h"

AASSkillProjectileActor::AASSkillProjectileActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
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
	bool bInCanHitPlayers,
	const FASCombatImpactContext& InImpactContext)
{
	TArray<FGameplayEffectSpecHandle> Specs;
	if (InDamageSpec.IsValid())
	{
		Specs.Add(InDamageSpec);
	}
	InitProjectile(Specs, InInstigatorAvatar, Direction, Speed, MaxRange, InPierceCount, bInCanHitPlayers, InImpactContext);
}

void AASSkillProjectileActor::InitProjectile(
	const TArray<FGameplayEffectSpecHandle>& InDamageSpecs,
	AActor* InInstigatorAvatar,
	const FVector& Direction,
	float Speed,
	float MaxRange,
	int32 InPierceCount,
	bool bInCanHitPlayers,
	const FASCombatImpactContext& InImpactContext)
{
	DamageSpecs = InDamageSpecs;
	InstigatorAvatar = InInstigatorAvatar;
	ImpactContext = InImpactContext;
	PierceCount = InPierceCount;
	bCanHitPlayers = bInCanHitPlayers;
	SetActorTickEnabled(true);
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
	if (ASCombatDiagnostics::IsEnabled())
	{
		ASCombatDiagnostics::Record(this, FString::Printf(TEXT("Projectile.Begin TickEnabled=%d StartEnabled=%d"),
			IsActorTickEnabled(), PrimaryActorTick.bStartWithTickEnabled));
	}

	// Deferred Spawn의 InitProjectile 뒤에는 FinishSpawning 과정에서 BP Construction Script가 실행된다.
	// 그 스크립트가 Mesh/Collision 기본값을 다시 적용할 수 있으므로, 실제 비행 직전에 다시 강제한다.
	ConfigureProjectileCollision();

	if (HasAuthority())
	{
		if (UASCombatTargetSubsystem* Targets = GetWorld()->GetSubsystem<UASCombatTargetSubsystem>())
		{
			Targets->AddMassTickPrerequisites(PrimaryActorTick);
		}
		PreviousLocation = GetActorLocation();
		//최대 거리에 도달하면 사라짐
		GetWorldTimerManager().SetTimer(RangeTimerHandle, this, &ThisClass::HandleRangeExpired, FlightTime, false);
	}
}

void AASSkillProjectileActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bEnded)
	{
		return;
	}

	ResolveHitsAlongSegment(GetActorLocation());
}

void AASSkillProjectileActor::ResolveHitsAlongSegment(const FVector& CurrentLocation)
{
	if (bEnded || !bResolveSegmentHits)
	{
		return;
	}
	if (UASCombatTargetSubsystem* Targets = GetWorld()->GetSubsystem<UASCombatTargetSubsystem>())
	{
		if (Targets->DeferUntilMassIdle(DeferredHitTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			DeferredHitTimer.Invalidate();
			ResolveHitsAlongSegment(GetActorLocation());
		}))) return;
	}

	const int32 RemainingHits = PierceCount >= 0
		? FMath::Max(0, PierceCount + 1 - HitTargets.Num())
		: 0;
	if (PierceCount >= 0 && RemainingHits <= 0)
	{
		EndProjectile();
		return;
	}

	if (UASCombatTargetSubsystem* CombatTargets = GetWorld()->GetSubsystem<UASCombatTargetSubsystem>())
	{
		FASCombatTargetQuery Query;
		Query.Shape = EASCombatTargetShape::Segment;
		Query.SearchOrigin = PreviousLocation;
		Query.EndLocation = CurrentLocation;
		Query.SearchRadius = CollisionComponent ? CollisionComponent->GetScaledSphereRadius() : 20.0f;
		Query.Instigator = InstigatorAvatar.Get();
		Query.bCanHitPlayers = bCanHitPlayers;
		Query.MaxTargets = PierceCount >= 0 ? RemainingHits : 0;
		const FASCombatApplyResult Result = CombatTargets->ResolveAndApply(Query, DamageSpecs, ImpactContext, &HitTargets);
		ASSkillDamageDebug::Report(this, DamageSpecs, Result, CurrentLocation);

		if (Result.HasAnyHit())
		{
			OnTargetsHit(Result, CurrentLocation);
		}
	}

	PreviousLocation = CurrentLocation;
	if (PierceCount >= 0 && HitTargets.Num() > PierceCount)
	{
		EndProjectile();
	}
}

void AASSkillProjectileActor::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bEnded)
	{
		return;
	}

	ResolveHitsAlongSegment(GetActorLocation());
	if (bEnded)
	{
		return;
	}

	if (PierceCount >= 0 && HitTargets.Num() > PierceCount)
	{
		EndProjectile();
	}
}

void AASSkillProjectileActor::HandleStop(const FHitResult& ImpactResult)
{
	if (ASCombatDiagnostics::IsEnabled())
	{
		ASCombatDiagnostics::Record(this, FString::Printf(TEXT("Projectile.Stopped HitActor=%s Location=%s"),
			*GetNameSafe(ImpactResult.GetActor()), *ImpactResult.ImpactPoint.ToString()));
	}
	if (HasAuthority())
	{
		FinishFlight(ImpactResult.ImpactPoint);
	}
}

void AASSkillProjectileActor::HandleRangeExpired()
{
	FinishFlight(GetActorLocation());
}

void AASSkillProjectileActor::FinishFlight(const FVector FinalLocation)
{
	if (bEnded) return;
	if (UASCombatTargetSubsystem* Targets = GetWorld()->GetSubsystem<UASCombatTargetSubsystem>())
	{
		if (Targets->DeferUntilMassIdle(DeferredEndTimer,
			FTimerDelegate::CreateWeakLambda(this, [this, FinalLocation]()
			{
				DeferredEndTimer.Invalidate();
				FinishFlight(FinalLocation);
			}))) return;
	}
	ResolveHitsAlongSegment(FinalLocation);
	EndProjectile();
}

void AASSkillProjectileActor::OnHitTarget_Implementation(AActor* Target)
{
	UASCombatTargetSubsystem::ApplyEffectSpecsToActor(DamageSpecs, Target);
}

void AASSkillProjectileActor::OnProjectileEnd_Implementation()
{
}

void AASSkillProjectileActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//서버가 투사체를 지울 때 각 화면에서 직접 재생 (클라이언트는 복제된 투사체가 사라질 때, 레벨 종료 등은 제외)
	if (EndPlayReason == EEndPlayReason::Destroyed && ImpactFX && GetNetMode() != NM_DedicatedServer)
	{
		ASSkillFX::SpawnAtLocation(this, ImpactFX, GetActorLocation(), GetActorRotation(), FVector(ImpactFXScale));
	}

	Super::EndPlay(EndPlayReason);
}

void AASSkillProjectileActor::EndProjectile()
{
	if (bEnded)
	{
		return;
	}

	bEnded = true;
	GetWorldTimerManager().ClearTimer(RangeTimerHandle);
	GetWorldTimerManager().ClearTimer(DeferredHitTimer);
	GetWorldTimerManager().ClearTimer(DeferredEndTimer);

	OnProjectileFinishedNative();
	OnProjectileEnd();
	Destroy();
}
