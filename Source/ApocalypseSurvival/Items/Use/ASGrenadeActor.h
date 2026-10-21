#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ASGrenadeActor.generated.h"

class UAbilitySystemComponent;
class UASItemInstance;
class UGameplayEffect;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

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
	float FuseTime = 3.0f;

	/** Positive fallback damage. An item's negative Data.Damage SetByCaller value overrides this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade", meta = (ClampMin = "0.0"))
	float Damage = 50.0f;

	/** Must use the project's negative Data.Damage contract to also damage Mass monsters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Visual")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Visual")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Visual")
	bool bDrawDebugExplosion = false;

private:
	void ConfigureCollision();

	UFUNCTION()
	void HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplosion(FVector_NetQuantize Location);

	/** Keep the last inventory unit available to effects for the entire fuse. */
	UPROPERTY(Transient)
	TObjectPtr<UASItemInstance> SourceItem;

	/** Snapshot of the item PDA at throw time. */
	UPROPERTY(Replicated)
	float ExplosionRadius = 300.0f;

	FGameplayEffectSpecHandle DamageSpec;
	TWeakObjectPtr<AActor> InstigatorAvatar;
	FVector InitialVelocity = FVector::ZeroVector;
	FTimerHandle FuseTimerHandle;
	bool bInitialized = false;
	bool bThrown = false;
	bool bExploded = false;
};
