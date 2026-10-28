#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterPatternAbility.h"
#include "ASMonsterSlowFieldAbility.generated.h"

class AASMonsterAttackIndicatorActor;
class AASMonsterSlowFieldActor;
class AASMonsterSlowFieldProjectile;
class UGameplayEffect;

/** 플레이어 이동 경로를 예측해 둔화 장판을 투척하는 엘리트 B 전용 패턴. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterSlowFieldAbility : public UASMonsterPatternAbility
{
	GENERATED_BODY()

public:
	UASMonsterSlowFieldAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void BeginPatternWindup() override;
	virtual bool ExecutePatternEffect() override;
	virtual void EndPattern(bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField")
	TSubclassOf<AASMonsterSlowFieldProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField")
	TSubclassOf<AASMonsterSlowFieldActor> SlowFieldClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField")
	TSubclassOf<AASMonsterAttackIndicatorActor> LandingIndicatorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField")
	TSubclassOf<UGameplayEffect> SlowEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField", meta = (ClampMin = "0.0", Units = "s"))
	float PredictionLeadTime = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxThrowDistance = 1700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField", meta = (ClampMin = "0.05", Units = "s"))
	float ProjectileTravelDuration = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField", meta = (ClampMin = "0.0", Units = "cm"))
	float ProjectileArcHeight = 280.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField", meta = (ClampMin = "1.0", Units = "cm"))
	float FieldRadius = 280.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|SlowField", meta = (ClampMin = "0.05", Units = "s"))
	float FieldDuration = 5.0f;

private:
	bool LockTargetAndPredictLandingLocation();
	FVector ClampLandingLocationToThrowRange(const FVector& CandidateLocation) const;
	void SpawnLandingIndicator();
	void DestroyLandingIndicator();

	TWeakObjectPtr<AActor> LockedTarget;
	FVector LandingLocation = FVector::ZeroVector;
	bool bHasLandingLocation = false;
	TWeakObjectPtr<AASMonsterAttackIndicatorActor> LandingIndicator;
};
