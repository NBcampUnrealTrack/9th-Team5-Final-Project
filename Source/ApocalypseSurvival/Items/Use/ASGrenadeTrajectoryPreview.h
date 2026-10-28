#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ASGrenadeTrajectoryPreview.generated.h"

class AASGrenadeActor;
class AASSkillRangeIndicatorActor;
class APawn;
class ULocalPlayer;
class UProjectileMovementComponent;
class USphereComponent;
class SASGrenadeTrajectoryWidget;

/** Local aiming visual. Displays the grenade's trajectory only up to its first ground contact. */
UCLASS(NotBlueprintable, Transient)
class APOCALYPSESURVIVAL_API AASGrenadeTrajectoryPreview : public AActor
{
	GENERATED_BODY()

public:
	AASGrenadeTrajectoryPreview();
	bool InitializePreview(APawn* InCaster, TSubclassOf<AASGrenadeActor> GrenadeClass,
		float InFlightTime, const FVector& InSpawnOffset, AASSkillRangeIndicatorActor* InIndicator = nullptr);
	bool PredictTrajectory(const FVector& GroundTarget, TArray<FVector>& OutPath, FVector& OutExplosionLocation);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdatePreview();

	UFUNCTION()
	void HandlePredictionBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UPROPERTY(Transient)
	TObjectPtr<USphereComponent> SimulationCollision;
	UPROPERTY(Transient)
	TObjectPtr<UProjectileMovementComponent> SimulationMovement;
	TSharedPtr<SASGrenadeTrajectoryWidget> TrajectoryWidget;
	TWeakObjectPtr<ULocalPlayer> PreviewPlayer;

	TWeakObjectPtr<APawn> Caster;
	TWeakObjectPtr<AASSkillRangeIndicatorActor> Indicator;
	FVector SpawnOffset = FVector::ZeroVector;
	float FlightTime = 0.8f;
	float FuseTime = 3.0f;
	FVector FirstGroundContact = FVector::ZeroVector;
	bool bReachedGround = false;
};
