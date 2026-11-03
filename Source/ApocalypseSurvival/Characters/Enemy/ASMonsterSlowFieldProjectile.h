#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Actor.h"
#include "ASMonsterSlowFieldProjectile.generated.h"

class AASMonsterSlowFieldActor;
class USceneComponent;

/**
 * 둔화 장판 투척의 서버 권한 포물선 투사체.
 * 실제 충돌 위치가 아니라 Ability가 확정한 예측 착탄점으로 이동하므로 네트워크마다 동일한 장판이 생성된다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASMonsterSlowFieldProjectile : public AActor
{
	GENERATED_BODY()

public:
	AASMonsterSlowFieldProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Deferred Spawn의 FinishSpawning 전에 서버가 호출한다. */
	void InitializeProjectile(const FVector& InImpactLocation, float InTravelDuration, float InArcHeight,
		const FGameplayEffectSpecHandle& InSlowEffectSpec, AActor* InInstigatorAvatar,
		TSubclassOf<AASMonsterSlowFieldActor> InSlowFieldClass, float InFieldRadius, float InFieldDuration);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SlowField")
	void BP_OnProjectileImpact();

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastProjectileImpact(FVector_NetQuantize Location);

	void ArriveAtImpact();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	FVector StartLocation = FVector::ZeroVector;
	FVector ImpactLocation = FVector::ZeroVector;
	float TravelDuration = 0.65f;
	float ArcHeight = 280.0f;
	float ElapsedTravelTime = 0.0f;
	bool bInitialized = false;
	bool bArrived = false;
	bool bImpactFeedbackPlayed = false;

	FGameplayEffectSpecHandle SlowEffectSpec;
	TWeakObjectPtr<AActor> InstigatorAvatar;
	TSubclassOf<AASMonsterSlowFieldActor> SlowFieldClass;
	float FieldRadius = 280.0f;
	float FieldDuration = 5.0f;
};
