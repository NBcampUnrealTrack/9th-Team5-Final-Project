#include "Items/Use/ASGrenadeTrajectoryPreview.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SphereComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/ScopedMovementUpdate.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/Use/ASGrenadeActor.h"
#include "Items/Use/ASGrenadeTrajectoryWidget.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"

AASGrenadeTrajectoryPreview::AASGrenadeTrajectoryPreview()
{
	bReplicates = false;
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SimulationCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SimulationCollision"));
	SimulationCollision->SetupAttachment(RootComponent);
	SimulationCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SimulationCollision->SetGenerateOverlapEvents(false);
	SimulationCollision->SetCanEverAffectNavigation(false);
	SimulationCollision->SetVisibility(false);

}

bool AASGrenadeTrajectoryPreview::InitializePreview(APawn* InCaster, TSubclassOf<AASGrenadeActor> GrenadeClass,
	const float InFlightTime, const FVector& InSpawnOffset, AASSkillRangeIndicatorActor* InIndicator)
{
	const AASGrenadeActor* Template = GrenadeClass ? GrenadeClass->GetDefaultObject<AASGrenadeActor>() : nullptr;
	UProjectileMovementComponent* Movement = Template ? Template->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
	const USphereComponent* Collision = Template ? Template->FindComponentByClass<USphereComponent>() : nullptr;
	if (!InCaster || SimulationMovement || !Movement || !Collision || InSpawnOffset.ContainsNaN()
		|| !FMath::IsFinite(InFlightTime) || InFlightTime < 0.05f
		|| !FMath::IsFinite(Template->GetFuseTime()) || Template->GetFuseTime() <= 0.0f) return false;
	Caster = InCaster;
	Indicator = InIndicator;
	FlightTime = InFlightTime;
	SpawnOffset = InSpawnOffset;
	FuseTime = Template->GetFuseTime();
	SimulationCollision->SetSphereRadius(Collision->GetScaledSphereRadius());
	// Copy authored BP movement settings instead of maintaining a second bounce/friction model.
	SimulationMovement = NewObject<UProjectileMovementComponent>(this, Movement->GetClass(), NAME_None, RF_Transient, Movement);
	SimulationMovement->OnProjectileBounce.Clear();
	SimulationMovement->OnProjectileBounce.AddDynamic(this, &ThisClass::HandlePredictionBounce);
	SimulationMovement->OnProjectileStop.Clear();
	SimulationMovement->bAutoActivate = false;
	SimulationMovement->bAutoUpdateTickRegistration = false;
	SimulationMovement->SetUpdatedComponent(SimulationCollision);
	SimulationMovement->RegisterComponent();
	SimulationMovement->SetComponentTickEnabled(false);
	if (APlayerController* PC = Cast<APlayerController>(InCaster->GetController()))
	{
		if (ULocalPlayer* Player = PC->GetLocalPlayer(); Player && Player->ViewportClient)
		{
			PreviewPlayer = Player;
			TrajectoryWidget = SNew(SASGrenadeTrajectoryWidget);
			Player->ViewportClient->AddViewportWidgetForPlayer(Player, TrajectoryWidget.ToSharedRef(), 5);
		}
	}
	if (InIndicator)
	{
		AddTickPrerequisiteActor(InIndicator);
		UpdatePreview();
	}
	return true;
}

bool AASGrenadeTrajectoryPreview::PredictTrajectory(const FVector& GroundTarget, TArray<FVector>& OutPath,
	FVector& OutExplosionLocation)
{
	OutPath.Reset();
	APawn* Avatar = Caster.Get();
	if (!IsValid(Avatar) || !SimulationMovement || GroundTarget.ContainsNaN()) return false;
	const FTransform Launch = AASGrenadeActor::MakeThrowTransform(Avatar, GroundTarget, SpawnOffset);
	const float Radius = SimulationCollision->GetScaledSphereRadius();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASGrenadePreviewSpawn), false, Avatar);
	FHitResult Obstruction;
	if (GetWorld()->LineTraceSingleByChannel(Obstruction, Avatar->GetActorLocation(), Launch.GetLocation(), ECC_Visibility, Params)) return false;
	SimulationCollision->SetWorldTransform(Launch, false, nullptr, ETeleportType::TeleportPhysics);
	AASGrenadeActor::ConfigureGrenadeCollision(SimulationCollision);
	SimulationMovement->SetUpdatedComponent(SimulationCollision);
	SimulationMovement->PreviousHitTime = 1.0f;
	SimulationMovement->PreviousHitNormal = FVector::UpVector;
	SimulationMovement->bIsSliding = false;
	bReachedGround = false;
	SimulationMovement->ClearPendingForce(true);
	SimulationMovement->Velocity = AASGrenadeActor::MakeThrowVelocity(Launch.GetLocation(), GroundTarget,
		Radius, SimulationMovement->GetGravityZ(), FlightTime);
	SimulationMovement->Activate(true);
	SimulationMovement->SetComponentTickEnabled(false);
	OutPath.Add(Launch.GetLocation());
	{
		// Discard all deferred hit/overlap notifications: aiming must never activate world gameplay.
		FScopedMovementUpdate PredictionScope(SimulationCollision, EScopedUpdate::DeferredUpdates);
		float RemainingTime = FuseTime;
		bool bCollectPath = true;
		while (RemainingTime > UE_KINDA_SMALL_NUMBER && SimulationMovement->UpdatedComponent)
		{
			const float Step = FMath::Min(RemainingTime, 1.0f / 60.0f);
			SimulationMovement->TickComponent(Step, LEVELTICK_All, nullptr);
			if (bCollectPath)
			{
				// Keep the exact contact point rather than the end of a step that may already have bounced.
				OutPath.Add(bReachedGround ? FirstGroundContact : SimulationCollision->GetComponentLocation());
				bCollectPath = !bReachedGround;
			}
			RemainingTime -= Step;
		}
		OutExplosionLocation = SimulationCollision->GetComponentLocation();
		PredictionScope.RevertMove();
	}
	SimulationMovement->SetUpdatedComponent(nullptr);
	SimulationCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	return !OutExplosionLocation.ContainsNaN();
}

void AASGrenadeTrajectoryPreview::HandlePredictionBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	// Vertical walls can deflect the flight; stop drawing when it first lands on an upward-facing surface.
	if (!bReachedGround && ImpactResult.ImpactNormal.Z >= 0.5f)
	{
		bReachedGround = true;
		FirstGroundContact = ImpactResult.Location;
	}
}

void AASGrenadeTrajectoryPreview::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Caster.IsValid() || !Indicator.IsValid())
	{
		Destroy();
		return;
	}
	UpdatePreview();
}

void AASGrenadeTrajectoryPreview::UpdatePreview()
{
	TArray<FVector> Path;
	FVector ExplosionLocation = FVector::ZeroVector;
	PredictTrajectory(Indicator->GetAimTargetLocation(), Path, ExplosionLocation);
	if (!TrajectoryWidget.IsValid()) return;
	const APlayerController* PC = Cast<APlayerController>(Caster->GetController());
	TArray<FVector2D> ScreenPoints;
	for (const FVector& Position : Path)
	{
		FVector2D ScreenPosition;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Position, ScreenPosition, true))
		{
			ScreenPoints.Add(ScreenPosition);
		}
	}
	TrajectoryWidget->SetPoints(MoveTemp(ScreenPoints));
}

void AASGrenadeTrajectoryPreview::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULocalPlayer* Player = PreviewPlayer.Get(); Player && Player->ViewportClient && TrajectoryWidget.IsValid())
	{
		Player->ViewportClient->RemoveViewportWidgetForPlayer(Player, TrajectoryWidget.ToSharedRef());
	}
	TrajectoryWidget.Reset();
	Super::EndPlay(EndPlayReason);
}
