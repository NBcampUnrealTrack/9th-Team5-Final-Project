#pragma once

#include "CoreMinimal.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
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
	virtual void Tick(float DeltaSeconds) override;

	//생성 직후(FinishSpawning 전)에 호출
	void InitProjectile(
		const FGameplayEffectSpecHandle& InDamageSpec,
		AActor* InInstigatorAvatar,
		const FVector& Direction,
		float Speed,
		float MaxRange,
		int32 InPierceCount,
		bool bInCanHitPlayers,
		const FASCombatImpactContext& InImpactContext = FASCombatImpactContext());

	/** 여러 무기 피해 효과를 하나의 투사체에 실을 때 쓰는 초기화 경로. */
	void InitProjectile(
		const TArray<FGameplayEffectSpecHandle>& InDamageSpecs,
		AActor* InInstigatorAvatar,
		const FVector& Direction,
		float Speed,
		float MaxRange,
		int32 InPierceCount,
		bool bInCanHitPlayers,
		const FASCombatImpactContext& InImpactContext = FASCombatImpactContext());

protected:
	virtual void BeginPlay() override;

	// Actor 전용 레거시 훅. 효과 적용은 UASCombatTargetSubsystem이 담당한다.
	UFUNCTION(BlueprintNativeEvent, Category = "AS|Skill|Projectile", meta = (DeprecatedFunction, DeprecationMessage = "Use the unified combat target path. This event is no longer called by AASSkillProjectileActor."))
	void OnHitTarget(AActor* Target);

	//사라지기 직전 (폭발 등 확장용, 서버)
	UFUNCTION(BlueprintNativeEvent, Category = "AS|Skill|Projectile")
	void OnProjectileEnd();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	//적을 맞힌 순간 (서버). 자식 클래스에서 퍼짐·폭발 등 확장용. HitLocation = 맞힌 순간 투사체 위치
	virtual void OnTargetsHit(const FASCombatApplyResult& Result, const FVector& HitLocation) {}

	/** Native lifecycle hook that cannot be skipped by a Blueprint OnProjectileEnd override. */
	virtual void OnProjectileFinishedNative() {}

	bool CanHitPlayers() const { return bCanHitPlayers; }

	//false = 지나가는 길의 적 판정을 안 함 (유도 구체처럼 자식 클래스가 직접 피해를 줄 때)
	bool bResolveSegmentHits = true;

	//사라짐 처리 (자식 클래스에서도 호출)
	void EndProjectile();

	TArray<FGameplayEffectSpecHandle> DamageSpecs;
	TWeakObjectPtr<AActor> InstigatorAvatar;
	FASCombatImpactContext ImpactContext;

	// Actor/Mass 등 실제 Backend 종류와 관계없는 중복 피격 기록.
	FASCombatTargetHitSet HitTargets;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	/** 발사 직전에 BP 기본값을 덮어써 피격용 Sphere만 충돌에 참여하게 한다. */
	void ConfigureProjectileCollision();
	void ResolveHitsAlongSegment(const FVector& CurrentLocation);
	void FinishFlight(FVector FinalLocation);
	void HandleRangeExpired();

	//0 = 첫 적에서 사라짐, -1 = 무한 관통
	int32 PierceCount = 0;
	bool bCanHitPlayers = false;
	bool bEnded = false;
	float FlightTime = 1.0f;
	FVector PreviousLocation = FVector::ZeroVector;

	FTimerHandle RangeTimerHandle;
	FTimerHandle DeferredHitTimer;
	FTimerHandle DeferredEndTimer;
};
