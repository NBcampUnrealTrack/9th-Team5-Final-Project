#include "Items/Use/ASGrenadeActor.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/AssetManager.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Data/ASItemTags.h"
#include "Items/GAS/ASItemEffects.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Skills/ASSkillTags.h"
#include "TimerManager.h"

AASGrenadeActor::AASGrenadeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(15.0f);
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(8.0f);
	RootComponent = CollisionComponent;
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetIsReplicated(true);
	MeshAsset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	MeshComponent->SetRelativeScale3D(FVector(0.12f));
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.35f;
	ProjectileMovement->Friction = 0.3f;
	ProjectileMovement->bBounceAngleAffectsFriction = true;
	ProjectileMovement->MinFrictionFraction = 0.25f;
	ProjectileMovement->bForceSubStepping = true;
	ProjectileMovement->MaxSimulationTimeStep = 1.0f / 60.0f;
	ProjectileMovement->MaxSimulationIterations = 8;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 20.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->OnProjectileBounce.AddDynamic(this, &AASGrenadeActor::HandleBounce);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AASGrenadeActor::HandleProjectileStop);
	DamageEffectClass = UASItemDamageEffect::StaticClass();
	ConfigureCollision();
}

void AASGrenadeActor::ConfigureGrenadeCollision(USphereComponent* Sphere)
{
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Sphere->SetGenerateOverlapEvents(false);
	Sphere->SetCanEverAffectNavigation(false);
}

FTransform AASGrenadeActor::MakeThrowTransform(const APawn* Avatar, const FVector& TargetLocation, const FVector& SpawnOffset)
{
	FVector Direction = (TargetLocation - Avatar->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero()) Direction = Avatar->GetActorForwardVector().GetSafeNormal2D();
	const FRotator Facing = Direction.Rotation();
	FVector Offset = SpawnOffset;
	if (Offset.X > 0.0f) Offset.X = FMath::Min(Offset.X, FVector::Dist2D(TargetLocation, Avatar->GetActorLocation()) * 0.5f);
	return FTransform(Facing, Avatar->GetActorLocation() + Facing.RotateVector(Offset));
}

FVector AASGrenadeActor::MakeThrowVelocity(const FVector& Start, const FVector& GroundTarget,
	const float Radius, const float GravityZ, const float FlightTime)
{
	FVector Velocity = (GroundTarget + FVector(0.0f, 0.0f, Radius) - Start) / FlightTime;
	Velocity.Z -= 0.5f * GravityZ * FlightTime;
	return Velocity;
}

void AASGrenadeActor::ConfigureCollision()
{
	ConfigureGrenadeCollision(CollisionComponent);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive != CollisionComponent)
		{
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Primitive->SetGenerateOverlapEvents(false);
			Primitive->SetCanEverAffectNavigation(false);
		}
	}
}

void AASGrenadeActor::BeginPlay()
{
	Super::BeginPlay();
	ResourceRequest.Request(ASItemAssetLoading::GatherObject(*this),
		FStreamableDelegate::CreateWeakLambda(this, [this]() { ApplyLoadedResources(); }));
	ResourceRequest.TrackLoadingScreen(this, NSLOCTEXT("ASItemLoading", "GrenadeResources", "Preparing grenade resources..."));
	ApplyLoadedResources();
	ConfigureCollision();
	// Clients display replicated server movement instead of running a second projectile simulation.
	ProjectileMovement->Deactivate();
	UpdateFlightFeedback();
}

void AASGrenadeActor::ApplyLoadedResources()
{
	if (IsActorBeingDestroyed()) return;
	const UASItemDefinition* Definition = SourceItem ? SourceItem->GetItemDefinition() : nullptr;
	UStaticMesh* Mesh = Definition && Definition->WorldMesh.Get() ? Definition->WorldMesh.Get() : MeshAsset.Get();
	if (Mesh) MeshComponent->SetStaticMesh(Mesh);
	UpdateFlightFeedback();
}

bool AASGrenadeActor::InitializeGrenade(UAbilitySystemComponent* SourceASC, UASItemInstance* InSourceItem,
	const FVector& Velocity)
{
	if (!HasAuthority() || bInitialized || !IsValid(SourceASC) || !IsValid(InSourceItem)
		|| !IsValid(InSourceItem->GetItemDefinition()) || !DamageEffectClass.Get()
		|| DamageEffectClass.Get()->HasAnyClassFlags(CLASS_Abstract)
		|| !FMath::IsFinite(FuseTime) || FuseTime <= 0.0f
		|| !FMath::IsFinite(Damage) || Damage < 0.0f || Velocity.ContainsNaN()) return false;
	const UASItemDefinition* Definition = InSourceItem->GetItemDefinition();
	const float Radius = Definition->GetEffectMagnitude(ASGameplayTags::Data_Item_ExplosionRadius, ASItemMagnitudeDefaults::ExplosionRadius);
	if (!FMath::IsFinite(Radius) || Radius <= 0.0f) return false;
	for (const auto& Magnitude : Definition->EffectMagnitudes)
	{
		if (!Magnitude.Key.IsValid() || !FMath::IsFinite(Magnitude.Value)) return false;
	}
	const float* ItemDamage = Definition->EffectMagnitudes.Find(ASGameplayTags::Data_Damage);
	// Positive Data.Damage would heal actors while Mass expects negative damage. Reject it before consumption.
	if (ItemDamage && *ItemDamage > 0.0f) return false;
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(InSourceItem);
	DamageSpec = SourceASC->MakeOutgoingSpec(DamageEffectClass.Get(), 1.0f, Context);
	if (!DamageSpec.IsValid()) return false;
	DamageSpec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -Damage);
	for (const auto& Magnitude : Definition->EffectMagnitudes)
	{
		DamageSpec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
	}
	if (UStaticMesh* ItemMesh = Definition->WorldMesh.Get())
	{
		MeshComponent->SetStaticMesh(ItemMesh);
		MeshComponent->SetRelativeScale3D(FVector::OneVector);
	}
	else if (!Definition->WorldMesh.IsNull())
	{
		const TWeakObjectPtr<const UASItemDefinition> WeakDefinition(Definition);
		MeshAssetRequest.Request(Definition->WorldMesh.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this,
			[this, WeakDefinition]
			{
				if (bExploded || IsActorBeingDestroyed() || !WeakDefinition.IsValid()) return;
				if (UStaticMesh* ItemMesh = WeakDefinition->WorldMesh.Get())
				{
					MeshComponent->SetStaticMesh(ItemMesh);
					MeshComponent->SetRelativeScale3D(FVector::OneVector);
				}
			}));
		MeshAssetRequest.TrackLoadingScreen(this, NSLOCTEXT("ASItemLoading", "Grenade", "Preparing grenade appearance..."));
	}
	SourceItem = InSourceItem;
	InstigatorAvatar = SourceASC->GetAvatarActor();
	InitialVelocity = Velocity;
	ExplosionRadius = Radius;
	ConfigureCollision();
	bInitialized = true;
	return true;
}

void AASGrenadeActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASGrenadeActor, ExplosionRadius);
	DOREPLIFETIME(AASGrenadeActor, bThrown);
	DOREPLIFETIME(AASGrenadeActor, bInFlight);
	DOREPLIFETIME(AASGrenadeActor, bExploded);
}

void AASGrenadeActor::HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (!HasAuthority() || !bThrown || bExploded || !BounceSound.Get()
		|| ImpactVelocity.ContainsNaN() || ImpactResult.ImpactNormal.ContainsNaN()) return;
	const float ImpactSpeed = FMath::Abs(FVector::DotProduct(ImpactVelocity, ImpactResult.ImpactNormal.GetSafeNormal()));
	const double Now = GetWorld()->GetTimeSeconds();
	if (ImpactSpeed < MinBounceSoundSpeed
		|| (LastBounceSoundTime >= 0.0 && Now - LastBounceSoundTime < BounceSoundInterval)) return;
	LastBounceSoundTime = Now;
	MulticastBounceFeedback(ImpactResult.ImpactPoint, FMath::Clamp(ImpactSpeed / 600.0f, 0.2f, 1.0f));
	// ProjectileMovement retains tangential velocity, bounces, and eventually stops through friction.
}

void AASGrenadeActor::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || !bInFlight) return;
	bInFlight = false;
	UpdateFlightFeedback();
	ForceNetUpdate();
}

void AASGrenadeActor::OnRepFlightState()
{
	UpdateFlightFeedback();
}

void AASGrenadeActor::UpdateFlightFeedback()
{
	if (bExploded)
	{
		MeshComponent->SetVisibility(false, true);
		SetActorEnableCollision(false);
		ProjectileMovement->Deactivate();
	}
	if (GetNetMode() == NM_DedicatedServer) return;
	if (bThrown && bInFlight && !bExploded && TrailEffect.Get())
	{
		if (!IsValid(TrailComponent))
		{
			TrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(TrailEffect.Get(), CollisionComponent,
				NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
				true, true, ENCPoolMethod::None, false);
		}
	}
	else if (IsValid(TrailComponent))
	{
		// Stop spawning but let existing particles fade; an explosion clears the trail immediately.
		if (bExploded) TrailComponent->DeactivateImmediate();
		else TrailComponent->Deactivate();
	}
}

void AASGrenadeActor::MulticastThrowFeedback_Implementation(const FVector_NetQuantize Location)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (PinPullSound.Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, PinPullSound.Get(), Location,
			1.0f, 1.0f, 0.0f, SoundAttenuation.Get());
	}
	if (ThrowSound.Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, ThrowSound.Get(), Location,
			1.0f, 1.0f, 0.0f, SoundAttenuation.Get());
	}
}

void AASGrenadeActor::MulticastBounceFeedback_Implementation(const FVector_NetQuantize Location, const float Volume)
{
	if (GetNetMode() != NM_DedicatedServer && BounceSound.Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, BounceSound.Get(), Location,
			Volume, FMath::FRandRange(0.95f, 1.05f), 0.0f, SoundAttenuation.Get());
	}
}

void AASGrenadeActor::StartThrow()
{
	if (!HasAuthority() || !bInitialized || bThrown || bExploded) return;
	bThrown = true;
	bInFlight = true;
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->Velocity = InitialVelocity;
	ProjectileMovement->Activate(true);
	UpdateFlightFeedback();
	MulticastThrowFeedback(InstigatorAvatar.IsValid() ? InstigatorAvatar->GetActorLocation() : GetActorLocation());
	GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AASGrenadeActor::Explode, FuseTime, false);
	ForceNetUpdate();
}

void AASGrenadeActor::Explode()
{
	if (!HasAuthority() || !bThrown || bExploded) return;
	bExploded = true;
	bInFlight = false;
	GetWorldTimerManager().ClearTimer(FuseTimerHandle);
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	UpdateFlightFeedback();
	const FVector Location = GetActorLocation();
	if (UASCombatTargetSubsystem* Combat = GetWorld()->GetSubsystem<UASCombatTargetSubsystem>())
	{
		FASCombatTargetQuery Query;
		Query.SearchOrigin = Location;
		Query.SearchRadius = ExplosionRadius;
		Query.Instigator = InstigatorAvatar.Get();
		Query.bHitAllTeams = true;
		Combat->ResolveAndApply(Query, MakeArrayView(&DamageSpec, 1));
	}
	MulticastExplosion(Location);
	// Leave time for the final movement and cosmetic RPC to reach relevant clients before destruction.
	ForceNetUpdate();
	SetLifeSpan(0.2f);
}

void AASGrenadeActor::MulticastExplosion_Implementation(const FVector_NetQuantize Location)
{
	// The RPC can arrive before replicated state, so hide the grenade immediately on every recipient.
	bExploded = true;
	bInFlight = false;
	UpdateFlightFeedback();
	if (GetNetMode() == NM_DedicatedServer) return;
	const float ReferenceRadius = FMath::IsFinite(ExplosionEffectReferenceRadius)
		? FMath::Max(ExplosionEffectReferenceRadius, 1.0f) : 100.0f;
	TArray<FSoftObjectPath> FeedbackPaths;
	for (const auto& Path : { ExplosionEffect.ToSoftObjectPath(), ExplosionSound.ToSoftObjectPath(), SoundAttenuation.ToSoftObjectPath() })
		if (!Path.IsNull()) FeedbackPaths.AddUnique(Path);
	// The grenade can be destroyed before a late-relevant client finishes loading cosmetics.
	// Keep this request independent of the actor; never delay authoritative damage/fuse timing.
	FStreamableDelegate Feedback = FStreamableDelegate::CreateLambda(
		[World = TWeakObjectPtr<UWorld>(GetWorld()), Effect = ExplosionEffect, Sound = ExplosionSound,
		 Attenuation = SoundAttenuation, Position = FVector(Location), Scale = ExplosionRadius / ReferenceRadius]()
		{
			UWorld* CurrentWorld = World.Get();
			if (!CurrentWorld || CurrentWorld->bIsTearingDown) return;
			if (Effect.Get()) UNiagaraFunctionLibrary::SpawnSystemAtLocation(CurrentWorld, Effect.Get(), Position, FRotator::ZeroRotator, FVector(Scale));
			if (Sound.Get()) UGameplayStatics::PlaySoundAtLocation(CurrentWorld, Sound.Get(), Position, 1.f, 1.f, 0.f, Attenuation.Get());
		});
	if (ASItemAssetLoading::AreLoaded(FeedbackPaths)) Feedback.ExecuteIfBound();
	else UAssetManager::GetStreamableManager().RequestAsyncLoad(FeedbackPaths, MoveTemp(Feedback));
	if (bDrawDebugExplosion)
	{
		DrawDebugSphere(GetWorld(), Location, ExplosionRadius, 24, FColor::Orange, false, 1.0f);
	}
}

void AASGrenadeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MeshAssetRequest.Reset();
	ResourceRequest.Reset();
	GetWorldTimerManager().ClearTimer(FuseTimerHandle);
	if (IsValid(TrailComponent)) TrailComponent->DeactivateImmediate();
	Super::EndPlay(EndPlayReason);
}
