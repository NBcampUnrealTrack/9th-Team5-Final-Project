#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "ASMonsterRadialProjectileVolleyAbility.generated.h"

class AASSkillProjectileActor;

/** 포효 선딜 뒤 여러 차례의 360도 투사체 파동을 발사하는 보스 광폭화 패턴. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterRadialProjectileVolleyAbility : public UASMonsterComboStepAbility
{
	GENERATED_BODY()

public:
	UASMonsterRadialProjectileVolleyAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility,
		bool bWasCancelled) override;

protected:
	/** 포효 및 첫 파동 전의 선딜. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (ClampMin = "0.0", Units = "s"))
	float RoarWindupDuration = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley")
	TSubclassOf<AASSkillProjectileActor> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (ClampMin = "1.0", Units = "cm/s"))
	float ProjectileSpeed = 1900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (ClampMin = "1.0", Units = "cm"))
	float ProjectileRange = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (ClampMin = "-1"))
	int32 ProjectilePierceCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (ClampMin = "1"))
	int32 ProjectileCount = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (ClampMin = "1"))
	int32 VolleyCount = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (ClampMin = "0.01", Units = "s"))
	float VolleyInterval = 0.2f;

	/** 각 후속 파동에 더할 각도. 보통 한 발 간격의 절반을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (Units = "deg"))
	float VolleyAngleOffset = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (Units = "cm"))
	float SpawnForwardOffset = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (Units = "cm"))
	float SpawnHeightOffset = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Radial Volley", meta = (ClampMin = "0.0"))
	float DamageScale = 1.0f;

private:
	void FireNextVolley();
	void SpawnProjectile(const FVector& Direction) const;
	FGameplayEffectSpecHandle MakeScaledDamageSpec() const;

	int32 CurrentVolleyIndex = 0;
	FTimerHandle RoarWindupTimerHandle;
	FTimerHandle VolleyTimerHandle;
};
