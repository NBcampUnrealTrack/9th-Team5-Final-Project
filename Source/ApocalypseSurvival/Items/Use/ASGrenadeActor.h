#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "Items/Data/ASItemAssetLoading.h"
#include "ASGrenadeActor.generated.h"

class UAbilitySystemComponent;
class UASItemInstance;
class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USoundAttenuation;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;
class APawn;

/** Server-simulated bouncing grenade. Its fuse applies one explosion to every team, including the thrower. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASGrenadeActor : public AActor
{
	GENERATED_BODY()

public:
	AASGrenadeActor();

	/** Prepare damage and visuals after FinishSpawning, before committing inventory cost. No fuse or movement starts here. */
	bool InitializeGrenade(UAbilitySystemComponent* SourceASC, UASItemInstance* InSourceItem, const FVector& Velocity);
	void StartThrow();

	/** Shared by the local trajectory preview and the authoritative throw. Target is already range/ground resolved. */
	static FTransform MakeThrowTransform(const APawn* Avatar, const FVector& TargetLocation, const FVector& SpawnOffset);
	static FVector MakeThrowVelocity(const FVector& Start, const FVector& GroundTarget, float Radius, float GravityZ, float FlightTime);
	static void ConfigureGrenadeCollision(USphereComponent* Sphere);
	float GetFuseTime() const { return FuseTime; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Item|Grenade")
	float GetExplosionRadius() const { return ExplosionRadius; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item|Grenade")
	void Explode();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade", meta = (ClampMin = "0.01", Units = "s"))
	float FuseTime = 1.5f;

	/** Positive fallback damage. An item's negative Data.Damage SetByCaller value overrides this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade", meta = (ClampMin = "0.0"))
	float Damage = 50.0f;

	/** Must use the project's negative Data.Damage contract to also damage Mass monsters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Played once at the thrower's location after item consumption succeeds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Audio")
	TObjectPtr<USoundBase> PinPullSound;

	/** Cloth/air movement at release, layered independently from the pin sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Audio")
	TObjectPtr<USoundBase> ThrowSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Audio")
	TObjectPtr<USoundBase> BounceSound;

	/** Optional shared 3D attenuation override. Null uses the individual sound's attenuation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Audio")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	/** Normal impact speed below this value makes no bounce sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Audio", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinBounceSoundSpeed = 75.0f;

	/** Prevent rapid repeated sounds while sliding on the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Audio", meta = (ClampMin = "0.0", Units = "s"))
	float BounceSoundInterval = 0.1f;

	/** A world-space ribbon/smoke system attached to the moving grenade; deactivated when it rests or explodes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Visual")
	TObjectPtr<UNiagaraSystem> TrailEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Visual")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	/** Radius of the authored effect at scale 1. The effect must support component scaling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Visual", meta = (ClampMin = "1.0", Units = "cm"))
	float ExplosionEffectReferenceRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Audio")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Visual")
	bool bDrawDebugExplosion = false;

private:
	void ConfigureCollision();
	void UpdateFlightFeedback();

	UFUNCTION()
	void OnRepFlightState();

	UFUNCTION()
	void HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastThrowFeedback(FVector_NetQuantize Location);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBounceFeedback(FVector_NetQuantize Location, float Volume);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplosion(FVector_NetQuantize Location);

	/** Keep the last inventory unit available to effects for the entire fuse. */
	UPROPERTY(Transient)
	TObjectPtr<UASItemInstance> SourceItem;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TrailComponent;
	FASItemAssetRequest MeshAssetRequest;

	/** Snapshot of the item PDA at throw time. */
	UPROPERTY(Replicated)
	float ExplosionRadius = 300.0f;

	FGameplayEffectSpecHandle DamageSpec;
	TWeakObjectPtr<AActor> InstigatorAvatar;
	FVector InitialVelocity = FVector::ZeroVector;
	FTimerHandle FuseTimerHandle;
	double LastBounceSoundTime = -1.0;
	bool bInitialized = false;

	UPROPERTY(ReplicatedUsing = OnRepFlightState)
	bool bThrown = false;

	UPROPERTY(ReplicatedUsing = OnRepFlightState)
	bool bInFlight = false;

	UPROPERTY(ReplicatedUsing = OnRepFlightState)
	bool bExploded = false;
};
