#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ASSkillProjectileActor.generated.h"

class UProjectileMovementComponent;
class USphereComponent;

//일자로 날아가며 맞은 적에게 데미지를 주는 투사체

UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASSkillProjectileActor : public AActor
{
	GENERATED_BODY()

public:
	AASSkillProjectileActor();

	//생성 직후(FinishSpawning 전)에 호출
	void InitProjectile(
		const FGameplayEffectSpecHandle& InDamageSpec,
		AActor* InInstigatorAvatar,
		const FVector& Direction,
		float Speed,
		float MaxRange,
		int32 InPierceCount,
		bool bInCanHitPlayers);

protected:
	virtual void BeginPlay() override;

	//적에게 맞았을 때 (기본: 데미지, 서버)
	UFUNCTION(BlueprintNativeEvent, Category = "AS|Skill|Projectile")
	void OnHitTarget(AActor* Target);

	//사라지기 직전 (폭발 등 확장용, 서버)
	UFUNCTION(BlueprintNativeEvent, Category = "AS|Skill|Projectile")
	void OnProjectileEnd();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	FGameplayEffectSpecHandle DamageSpec;
	TWeakObjectPtr<AActor> InstigatorAvatar;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	/** 발사 직전에 BP 기본값을 덮어써 피격용 Sphere만 충돌에 참여하게 한다. */
	void ConfigureProjectileCollision();

	void EndProjectile();

	//0 = 첫 적에서 사라짐, -1 = 무한 관통
	int32 PierceCount = 0;
	bool bCanHitPlayers = false;
	bool bEnded = false;
	float FlightTime = 1.0f;

	//같은 적 중복 피격 방지
	TSet<TWeakObjectPtr<AActor>> HitActors;

	FTimerHandle RangeTimerHandle;
};
