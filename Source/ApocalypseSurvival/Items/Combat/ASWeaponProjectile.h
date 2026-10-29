#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "ASWeaponProjectile.generated.h"

class USceneComponent;

/**
 * 평타 유도 투사체.
 * 서버가 대상(일반 몬스터 / Mass 몬스터)의 현재 위치를 따라 이동하고, 닿으면 그 대상에게 피해를 준다.
 * 충돌체 대신 거리로 명중을 판정한다 (Mass 몬스터는 충돌체가 없다). 위치는 복제되어 모든 클라이언트에 보인다.
 * 보이는 것(메시, Niagara)은 BP 자식에서 붙인다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASWeaponProjectile : public AActor
{
	GENERATED_BODY()

public:
	AASWeaponProjectile();

	virtual void Tick(float DeltaSeconds) override;

	/** 서버 전용. SpawnActorDeferred 후 FinishSpawning 전에 호출한다. */
	void InitHoming(const TArray<FGameplayEffectSpecHandle>& InDamageSpecs, AActor* InInstigatorAvatar,
		const FASCombatTargetRef& InTarget, float InSpeed,
		const FASCombatImpactContext& InImpactContext = FASCombatImpactContext());

protected:
	virtual void BeginPlay() override;

	/** Lets specialized homing projectiles replace the per-hit reaction without changing damage specs. */
	virtual FASCombatImpactContext ResolveImpactContextForTarget(
		const FASCombatTargetRef& InTarget, const FASCombatImpactContext& BaseContext) const;

	/** Called exactly once on authority when the homing projectile resolves as a hit or miss. */
	virtual void OnHomingProjectileFinished(bool bHit) {}

	/** 명중하거나 대상을 잃고 사라질 때 모든 머신에서 불린다. 명중 이펙트·사운드는 BP 에서 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
	void OnProjectileImpact(FVector ImpactLocation, bool bHit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 이 시간이 지나도 닿지 못하면 사라진다 (임시값) */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (ClampMin = "0.1", Units = "s"))
	float MaxLifetime = 3.0f;

	/** 대상 피격 반경에 더하는 명중 여유 거리 (임시값) */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (ClampMin = "0.0", Units = "cm"))
	float ImpactTolerance = 30.0f;

private:
	/** 서버 전용. 피해를 주고 사라진다. bHasTarget 이 false 면 피해 없이 사라진다. */
	void Impact(bool bHasTarget);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(FVector ImpactLocation, bool bHit);

	TArray<FGameplayEffectSpecHandle> DamageSpecs;
	TWeakObjectPtr<AActor> InstigatorAvatar;
	FASCombatImpactContext ImpactContext;

	/** 따라가는 대상. RefreshTarget 이 위치를 갱신하고, 대상이 죽으면 비운다. */
	FASCombatTargetRef Target;
	FVector LastTargetLocation = FVector::ZeroVector;

	float Speed = 2500.0f;
	float Elapsed = 0.0f;
	bool bInitialized = false;
	bool bImpacted = false;
};
