#include "Characters/Enemy/ASMonsterSlowFieldProjectile.h"

#include "Characters/Enemy/ASMonsterSlowFieldActor.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

AASMonsterSlowFieldProjectile::AASMonsterSlowFieldProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AASMonsterSlowFieldProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(HasAuthority() && bInitialized);
}

void AASMonsterSlowFieldProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bInitialized || bArrived)
	{
		return;
	}

	ElapsedTravelTime += DeltaSeconds;
	const float Alpha = FMath::Clamp(ElapsedTravelTime / FMath::Max(0.01f, TravelDuration), 0.0f, 1.0f);
	FVector Location = FMath::Lerp(StartLocation, ImpactLocation, Alpha);
	Location.Z += 4.0f * ArcHeight * Alpha * (1.0f - Alpha);
	SetActorLocation(Location);

	const FVector Tangent = (ImpactLocation - StartLocation).GetSafeNormal();
	if (!Tangent.IsNearlyZero())
	{
		SetActorRotation(Tangent.Rotation());
	}

	if (Alpha >= 1.0f)
	{
		ArriveAtImpact();
	}
}

void AASMonsterSlowFieldProjectile::InitializeProjectile(const FVector& InImpactLocation,
	const float InTravelDuration, const float InArcHeight, const FGameplayEffectSpecHandle& InSlowEffectSpec,
	AActor* InInstigatorAvatar, const TSubclassOf<AASMonsterSlowFieldActor> InSlowFieldClass,
	const float InFieldRadius, const float InFieldDuration)
{
	if (!HasAuthority())
	{
		return;
	}

	StartLocation = GetActorLocation();
	ImpactLocation = InImpactLocation;
	TravelDuration = FMath::Max(0.05f, InTravelDuration);
	ArcHeight = FMath::Max(0.0f, InArcHeight);
	SlowEffectSpec = InSlowEffectSpec;
	InstigatorAvatar = InInstigatorAvatar;
	SlowFieldClass = InSlowFieldClass;
	FieldRadius = FMath::Max(1.0f, InFieldRadius);
	FieldDuration = FMath::Max(0.05f, InFieldDuration);
	bInitialized = true;
	SetLifeSpan(TravelDuration + 1.0f);
}

void AASMonsterSlowFieldProjectile::ArriveAtImpact()
{
	if (bArrived)
	{
		return;
	}

	bArrived = true;
	SetActorLocation(ImpactLocation);
	BP_OnProjectileImpact();

	UWorld* World = GetWorld();
	if (World)
	{
		UClass* ClassToSpawn = SlowFieldClass ? SlowFieldClass.Get() : AASMonsterSlowFieldActor::StaticClass();
		const FTransform SpawnTransform(FRotator::ZeroRotator, ImpactLocation);
		AASMonsterSlowFieldActor* SlowField = World->SpawnActorDeferred<AASMonsterSlowFieldActor>(
			ClassToSpawn, SpawnTransform, InstigatorAvatar.Get(), Cast<APawn>(InstigatorAvatar.Get()),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (SlowField)
		{
			SlowField->InitializeField(SlowEffectSpec, InstigatorAvatar.Get(), FieldRadius, FieldDuration);
			SlowField->FinishSpawning(SpawnTransform);
		}
	}

	Destroy();
}
