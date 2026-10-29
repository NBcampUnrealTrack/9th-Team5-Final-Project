#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Core/Combat/ASCombatImpactTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/Data/ASItemTypes.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"
#include "ASWeaponAttackProfile.generated.h"

class UAnimMontage;
class UASItemDefinition;
class UASWeaponAttackExecution;
class AASWeaponProjectile;
class AASShotgunPelletProjectile;
class AASSniperPiercingProjectile;
class FDataValidationContext;

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
	/** ItemDefinition이 구체 프로필 타입을 Cast하지 않고 호환성을 확인하는 다형성 진입점. */
	virtual bool SupportsWeaponType(EASWeaponType WeaponType) const { return false; }

#if WITH_EDITOR
	/** 이 프로필의 공통·전용 설정을 ItemDefinition 문맥에서 검증한다. */
	virtual EDataValidationResult ValidateForItem(
		const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSubclassOf<UASWeaponAttackExecution> ExecutionClass;

	/** Single damages one best target; Area damages every valid target in the attack area. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	EASAttackTargetMode TargetMode = EASAttackTargetMode::Single;

	/** 이 공격이 명중한 대상에게 전달할 경직 프리셋. 피해량과 독립적으로 선택한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Impact")
	EASImpactPreset ImpactPreset = EASImpactPreset::None;

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
	virtual bool SupportsWeaponType(EASWeaponType WeaponType) const override
	{
		return WeaponType == EASWeaponType::Melee;
	}

#if WITH_EDITOR
	virtual EDataValidationResult ValidateForItem(
		const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const override;
#endif

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
	virtual bool SupportsWeaponType(EASWeaponType WeaponType) const override
	{
		return WeaponType == EASWeaponType::Ranged;
	}

#if WITH_EDITOR
	virtual EDataValidationResult ValidateForItem(
		const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const override;
#endif

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

	/** Directional Attack에서만 쓰는 직선 투사체 클래스. 유도 투사체의 시각 BP와 분리한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Directional")
	TSubclassOf<AASSkillProjectileActor> DirectionalProjectileClass;

	/** 방향성 투사체 모드에서만 쓰는 발사체 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (EditCondition = "!bUseAttackMoveTargeting", EditConditionHides))
	TSubclassOf<AASSkillProjectileActor> ProjectileClass;

	/** 직선 투사체 속도. Directional Attack에서는 bUseAttackMoveTargeting 값과 무관하게 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (ClampMin = "1.0", Units = "cm/s"))
	float ProjectileSpeed = 2000.0f;

	/** 0은 첫 적에서 소멸, -1은 무한 관통. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (ClampMin = "-1"))
	int32 ProjectilePierceCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile")
	FName ProjectileSpawnSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (Units = "cm"))
	float ProjectileSpawnForwardOffset = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged|Projectile",
		meta = (Units = "cm"))
	float ProjectileSpawnHeightOffset = 0.0f;

	/** Radius around the first ranged impact used only by Area attacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Ranged",
		meta = (ClampMin = "0.0", Units = "cm", EditCondition = "TargetMode == EASAttackTargetMode::Area", EditConditionHides))
	float AreaRadius = 200.0f;
};

/** Assault-rifle-only data for a server-authoritative timed burst. */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASRifleBurstAttackProfile : public UASRangedAttackProfile
{
	GENERATED_BODY()

public:
	UASRifleBurstAttackProfile();

#if WITH_EDITOR
	virtual EDataValidationResult ValidateForItem(
		const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const override;
#endif

	/** Number of projectiles fired by one paid primary attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Rifle Burst", meta = (ClampMin = "2", UIMin = "2"))
	int32 BurstCount = 3;

	/** Delay between consecutive projectiles in the same burst. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Rifle Burst",
		meta = (ClampMin = "0.01", Units = "s"))
	float BurstShotInterval = 0.08f;

	/** The final projectile gains FocusedImpactPreset after this many total hits on the same target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Rifle Burst",
		meta = (ClampMin = "2", UIMin = "2"))
	int32 FocusedHitThreshold = 2;

	/** Focus reward. Heavy resolves by the target's resistance tier; it is not physical displacement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Rifle Burst")
	EASImpactPreset FocusedImpactPreset = EASImpactPreset::Heavy;
};

/** Shotgun-only spread and crowd-control data. Each pellet deals the equipped weapon's BaseDamage. */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASShotgunAttackProfile : public UASRangedAttackProfile
{
	GENERATED_BODY()

public:
	UASShotgunAttackProfile();

#if WITH_EDITOR
	virtual EDataValidationResult ValidateForItem(
		const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Shotgun", meta = (ClampMin = "1", UIMin = "1"))
	int32 PelletCount = 8;

	/** Full horizontal angle covered by all pellets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Shotgun",
		meta = (ClampMin = "1.0", ClampMax = "180.0", Units = "deg"))
	float SpreadAngleDegrees = 35.0f;

	/** Normal resistance targets move once per shotgun attack; Elite/Boss do not move. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Shotgun",
		meta = (ClampMin = "0.0", Units = "cm"))
	float KnockbackDistance = 180.0f;

	/** First pellet to hit each target uses this impact. Later pellets only deal damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Shotgun")
	EASImpactPreset KnockbackImpactPreset = EASImpactPreset::Heavy;

	/** Must derive from AASShotgunPelletProjectile to preserve one-knockback-per-target behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Shotgun")
	TSubclassOf<AASShotgunPelletProjectile> PelletProjectileClass;
};

/** Sniper-only ordered piercing rules. Damage multipliers are indexed by hit order. */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASSniperAttackProfile : public UASRangedAttackProfile
{
	GENERATED_BODY()

public:
	UASSniperAttackProfile();

#if WITH_EDITOR
	virtual EDataValidationResult ValidateForItem(
		const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Sniper", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxPierceTargets = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Sniper",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditFixedSize))
	TArray<float> PierceDamageMultipliers = { 1.0f, 0.75f, 0.55f, 0.40f };

	/** Must derive from AASSniperPiercingProjectile to preserve ordered, unique pierce behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Sniper")
	TSubclassOf<AASSniperPiercingProjectile> SniperProjectileClass;
};
