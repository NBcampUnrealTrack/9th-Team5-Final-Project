#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ASWeaponAttackProfile.generated.h"

class UAnimMontage;
class UASItemDefinition;
class UASWeaponAttackExecution;
class UGameplayEffect;

/** Data and execution policy consumed by the character-owned primary attack ability. */
UCLASS(Abstract, BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASWeaponAttackProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSubclassOf<UASWeaponAttackExecution> ExecutionClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Animation", meta = (Categories = "Event"))
	FGameplayTag HitEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Animation")
	bool bScaleMontageToAttackInterval = true;
};

UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASMeleeAttackProfile : public UASWeaponAttackProfile
{
	GENERATED_BODY()

public:
	UASMeleeAttackProfile();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Melee", meta = (ClampMin = "0.0"))
	float StaminaCost = 10.0f;

	/** Instant Blueprint GE with an Add (Base) Stamina modifier using Data.StaminaCost SetByCaller. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Melee")
	TSubclassOf<UGameplayEffect> StaminaCostEffect;

	/** Optional duration effect (for example GE_StaminaRegen_Delay) applied after paying stamina. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Melee")
	TSubclassOf<UGameplayEffect> StaminaRegenDelayEffect;

	/** Zero derives the radius from AttackRange. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Melee", meta = (ClampMin = "0.0", Units = "cm"))
	float HitRadius = 0.0f;

	/** Zero places the hit sphere two radii in front of the character. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Melee", meta = (ClampMin = "0.0", Units = "cm"))
	float HitboxOffset = 0.0f;
};

UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASRangedAttackProfile : public UASWeaponAttackProfile
{
	GENERATED_BODY()

public:
	UASRangedAttackProfile();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged")
	TObjectPtr<UASItemDefinition> AmmoDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged", meta = (ClampMin = "1"))
	int32 AmmoPerShot = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged", meta = (ClampMin = "1.0"))
	float RangeMultiplier = 10.0f;
};
