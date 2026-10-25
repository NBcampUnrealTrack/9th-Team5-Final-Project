#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"
#include "ASWeaponAttackProfile.generated.h"

class UAnimMontage;
class UASItemDefinition;
class UASWeaponAttackExecution;
class UGameplayEffect;
class AASWeaponProjectile;

/** How many valid targets receive damage after an attack finds its hit location. */
UENUM(BlueprintType)
enum class EASAttackTargetMode : uint8
{
	Single UMETA(DisplayName = "Single Target"),
	Area UMETA(DisplayName = "Area")
};

/** Data and execution policy consumed by the character-owned primary attack ability. */
UCLASS(Abstract, BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASWeaponAttackProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSubclassOf<UASWeaponAttackExecution> ExecutionClass;

	/** Single damages one best target; Area damages every valid target in the attack area. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	EASAttackTargetMode TargetMode = EASAttackTargetMode::Single;

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

	/** 캐릭터 앞쪽 부채꼴 판정의 전체 각도 (임시값). 반경은 무기의 AttackRange */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Melee", meta = (ClampMin = "1.0", ClampMax = "360.0", Units = "deg"))
	float HitAngle = 60.0f;
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

	/** true면 A+클릭이 적을 선택·추적한다. false면 클릭 방향으로 투사체를 한 발 발사한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged")
	bool bUseAttackMoveTargeting = true;
	
	/** 타겟 지정(A+클릭) 모드의 유도 투사체 (Single 모드). 비어 있으면 기존 즉시 판정 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Homing",
		meta = (EditCondition = "bUseAttackMoveTargeting", EditConditionHides))
	TSubclassOf<AASWeaponProjectile> HomingProjectileClass;

	/** 유도 투사체 속도 (임시값) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Homing",
		meta = (ClampMin = "1.0", Units = "cm/s", EditCondition = "bUseAttackMoveTargeting", EditConditionHides))
	float HomingProjectileSpeed = 2500.0f;

	/** 방향성 투사체 모드에서만 쓰는 발사체 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (EditCondition = "!bUseAttackMoveTargeting", EditConditionHides))
	TSubclassOf<AASSkillProjectileActor> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (ClampMin = "1.0", Units = "cm/s", EditCondition = "!bUseAttackMoveTargeting", EditConditionHides))
	float ProjectileSpeed = 2000.0f;

	/** 0은 첫 적에서 소멸, -1은 무한 관통. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (ClampMin = "-1", EditCondition = "!bUseAttackMoveTargeting", EditConditionHides))
	int32 ProjectilePierceCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (EditCondition = "!bUseAttackMoveTargeting", EditConditionHides))
	FName ProjectileSpawnSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (Units = "cm", EditCondition = "!bUseAttackMoveTargeting", EditConditionHides))
	float ProjectileSpawnForwardOffset = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (Units = "cm", EditCondition = "!bUseAttackMoveTargeting", EditConditionHides))
	float ProjectileSpawnHeightOffset = 0.0f;

	/** Radius around the first ranged impact used only by Area attacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged",
		meta = (ClampMin = "0.0", Units = "cm", EditCondition = "TargetMode == EASAttackTargetMode::Area", EditConditionHides))
	float AreaRadius = 200.0f;
};
