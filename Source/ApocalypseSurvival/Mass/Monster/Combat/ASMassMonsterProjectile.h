#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ASMassMonsterProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;

/** ASC를 갖지 않는 Mass 원거리 몬스터가 발사하는 서버 권위 직선 투사체. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASMassMonsterProjectile : public AActor
{
	GENERATED_BODY()

public:
	AASMassMonsterProjectile();

	void InitializeProjectile(float InDamage, const FVector& InSourceLocation,
		const FVector& Direction, float Speed, float MaxRange);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	void EndProjectile();

	float Damage = 0.0f;
	FVector SourceLocation = FVector::ZeroVector;
	float FlightTime = 1.0f;
	bool bEnded = false;
	FTimerHandle RangeTimerHandle;
};
