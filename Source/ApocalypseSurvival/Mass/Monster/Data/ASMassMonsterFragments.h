#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MassEntityTypes.h"
#include "MassLODTypes.h"
#include "Mass/Monster/Data/ASMassMonsterTypes.h"
#include "ASMassMonsterFragments.generated.h"

class AASMassMonsterProjectile;
class AASMonsterAttackIndicatorActor;
class UMassEntityConfigAsset;

/** Entity마다 독립적으로 보유하는 현재 체력. 최대 체력은 종류별 공유 설정에 둔다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterHealthFragment : public FMassFragment
{
	GENERATED_BODY()

	float CurrentHealth = 100.0f;
	float DeathTimeRemaining = 0.0f;
	bool bDeathTimerStarted = false;
	/** 가장 최근에 피해를 준 플레이어의 Mass TargetId. 경험치 수령자 판정에만 쓴다. */
	int32 LastDamageInstigatorTargetId = INDEX_NONE;
};

/** 낮/밤 행동 모드와 현재 상위 행동 상태. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterBehaviorFragment : public FMassFragment
{
	GENERATED_BODY()

	EASMassMonsterState State = EASMassMonsterState::Idle;
	EASMassMonsterWorldBehavior WorldBehavior = EASMassMonsterWorldBehavior::DayTerritory;
};

/** 타깃 Snapshot을 참조하기 위한 식별자와 마지막으로 확인한 위치. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterTargetFragment : public FMassFragment
{
	GENERATED_BODY()

	int32 TargetId = INDEX_NONE;
	FVector CachedTargetLocation = FVector::ZeroVector;
	float Aggro = 0.0f;
	float RefreshTimeRemaining = 0.0f;
	bool bRefreshScheduleInitialized = false;

	bool HasTarget() const
	{
		return TargetId != INDEX_NONE;
	}

	void Reset()
	{
		TargetId = INDEX_NONE;
		CachedTargetLocation = FVector::ZeroVector;
		Aggro = 0.0f;
		RefreshTimeRemaining = 0.0f;
	}
};

/** 공격 쿨다운과 준비/회복 단계를 저장한다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterCombatFragment : public FMassFragment
{
	GENERATED_BODY()

	double NextAttackTimeSeconds = 0.0;
	double PhaseEndTimeSeconds = 0.0;
	EASMassMonsterAttackState AttackState = EASMassMonsterAttackState::Ready;
	FVector LockedTargetLocation = FVector::ZeroVector;
	FVector LockedDirection = FVector::ForwardVector;
	int32 LockedTargetId = INDEX_NONE;
	float ChargeDistanceTraveled = 0.0f;
	uint8 PatternActivationCount = 0;
	bool bPrimaryEffectApplied = false;
	bool bUseFallbackMelee = false;
};

/** 일시적인 일반 몬스터 강화 상태. 종류별 기본 수치는 Shared Config에 유지한다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterModifierFragment : public FMassFragment
{
	GENERATED_BODY()

	float MoveSpeedMultiplier = 1.0f;
	double HasteEndTimeSeconds = 0.0;
	FVector BaseVisualScale = FVector::OneVector;
	bool bBaseVisualScaleInitialized = false;
};

/** 낮 귀환의 기준이 되는 개체별 홈 위치와 추격 제한. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterHomeFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector HomeLocation = FVector::ZeroVector;
	float LeashRadius = 2000.0f;
	float ReturnAcceptanceRadius = 100.0f;
	float DetectionRadius = 1500.0f;
	float LoseTargetRadius = 2200.0f;
	float ReturnHealthPercentPerSecond = 0.0f;
	float ReturnMoveSpeedMultiplier = 1.0f;
};

/** NavMesh 경로 재요청을 Entity별로 분산하기 위한 런타임 상태. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterNavigationFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector LastRequestedGoal = FVector::ZeroVector;
	float PathRefreshTimeRemaining = 0.0f;
	bool bHasRequestedGoal = false;
};

/**
 * 각 머신의 전장의 안개 표시 상태. 네트워크 게임 상태가 아니라 로컬 Representation 전용 데이터다.
 * 밝은 시야 안쪽의 표시 영역에서는 표시하고, 숨김선 바깥과 시야 경계 Buffer에서는 ISM을 유지한 채 숨긴다.
 * Buffer 바깥에 일정 시간 머문 뒤에만 ISM을 제거해 경계에서의 재등록을 줄인다.
 * 숨김과 재표시 Fade는 Transform이 아니라 ISM Per Instance Custom Data 0번(0..1 표시 알파)으로 전달한다.
 */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterFogVisibilityFragment : public FMassFragment
{
	GENERATED_BODY()

	bool bInitialized = false;
	bool bVisibleToLocalTeam = false;
	bool bRetainedInFogBuffer = false;
	/** Material의 Per Instance Custom Data 0번으로 전달되는 현재 표시 알파. */
	float RenderVisibilityAlpha = 0.0f;
	float SecondsOutsideFogBuffer = 0.0f;
	bool bTemporarilyForcedOff = false;
	TEnumAsByte<EMassLOD::Type> SavedLOD = EMassLOD::Off;
};

/** 같은 종류의 일반 몬스터가 공유하는 읽기 전용 설정. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Identity")
	FName MonsterTypeId = NAME_None;

	/** 플레이어가 이 Mass 몬스터를 처치했을 때 지급할 기본 경험치. */
	UPROPERTY(EditAnywhere, Category = "Rewards", meta = (ClampMin = "0"))
	int32 ExperienceReward = 10;

	/** Actor 몬스터와 공유하는 몬스터 드랍 그룹. 비어 있으면 아이템을 드랍하지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Rewards", meta = (RowType = "/Script/ApocalypseSurvival.ASMonsterDropGroupRow"))
	FDataTableRowHandle DropGroup;

	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MoveSpeed = 300.0f;

	/** Actor 충돌체가 없는 Entity를 서버 공격 판정에서 확장할 수평 반경. */
	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (ClampMin = "0.0", Units = "cm"))
	float HitRadius = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackPower = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.01", Units = "s"))
	float AttackCooldown = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	EASMassMonsterAttackPattern AttackPattern = EASMassMonsterAttackPattern::Melee;

	/** 소환 횟수를 모두 쓴 사이렌이 사용할 약한 근접 공격 설정. */
	UPROPERTY(EditAnywhere, Category = "Combat|Fallback", meta = (ClampMin = "0.0", Units = "cm"))
	float FallbackMeleeAttackRange = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Fallback", meta = (ClampMin = "0.0"))
	float FallbackMeleeAttackPower = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Fallback", meta = (ClampMin = "0.01", Units = "s"))
	float FallbackMeleeAttackCooldown = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Combat|Ranged", meta = (ClampMin = "1.0", Units = "cm/s"))
	float RangedProjectileSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Ranged", meta = (ClampMin = "0.0", Units = "cm"))
	float RangedProjectileMaxRange = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Ranged", meta = (ClampMin = "0.0", Units = "cm"))
	float RangedProjectileSpawnHeight = 50.0f;

	/** 비어 있으면 네이티브 기본 투사체를 사용한다. 메시/VFX가 필요하면 Blueprint 자식을 지정한다. */
	UPROPERTY(EditAnywhere, Category = "Combat|Ranged")
	TSubclassOf<AASMassMonsterProjectile> RangedProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Combat|Charge", meta = (ClampMin = "0.0", Units = "s"))
	float ChargeWindupDuration = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Combat|Charge", meta = (ClampMin = "1.0", Units = "cm"))
	float ChargeDistance = 750.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Charge", meta = (ClampMin = "0.01", Units = "s"))
	float ChargeDuration = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Combat|Charge", meta = (ClampMin = "1.0", Units = "cm"))
	float ChargeHitRadius = 110.0f;

	/** 선딜 동안 돌진 경로를 표시한다. 보통 BP_Indicator_Charge를 지정한다. */
	UPROPERTY(EditAnywhere, Category = "Combat|Charge")
	TSubclassOf<AASMonsterAttackIndicatorActor> ChargeIndicatorClass;

	UPROPERTY(EditAnywhere, Category = "Combat|Suicide", meta = (ClampMin = "0.0", Units = "s"))
	float SuicideWindupDuration = 1.1f;

	UPROPERTY(EditAnywhere, Category = "Combat|Suicide", meta = (ClampMin = "1.0", Units = "cm"))
	float SuicideRadius = 320.0f;

	/** 선딜 동안 폭발 범위를 표시한다. 보통 BP_Indicator_Radial을 지정한다. */
	UPROPERTY(EditAnywhere, Category = "Combat|Suicide")
	TSubclassOf<AASMonsterAttackIndicatorActor> SuicideIndicatorClass;

	UPROPERTY(EditAnywhere, Category = "Combat|Summon")
	TObjectPtr<UMassEntityConfigAsset> SummonedEntityConfig;

	UPROPERTY(EditAnywhere, Category = "Combat|Summon", meta = (ClampMin = "1"))
	int32 SummonCount = 2;

	UPROPERTY(EditAnywhere, Category = "Combat|Summon", meta = (ClampMin = "1"))
	int32 MaxSummonActivations = 2;

	UPROPERTY(EditAnywhere, Category = "Combat|Summon", meta = (ClampMin = "0.0", Units = "cm"))
	float SummonMinRadius = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Summon", meta = (ClampMin = "1.0", Units = "cm"))
	float SummonMaxRadius = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Summon", meta = (ClampMin = "0.0", Units = "s"))
	float SummonWindupDuration = 1.4f;

	UPROPERTY(EditAnywhere, Category = "Combat|Haste", meta = (ClampMin = "1.0", Units = "cm"))
	float HasteRadius = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Haste", meta = (ClampMin = "1.0"))
	float HasteMoveSpeedMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, Category = "Combat|Haste", meta = (ClampMin = "0.0", Units = "s"))
	float HasteDuration = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Haste", meta = (ClampMin = "0.0", Units = "s"))
	float HasteWindupDuration = 0.6f;

	/** 선딜 동안 이속 강화 범위를 표시한다. 버프 전용 색상의 원형 Indicator를 지정한다. */
	UPROPERTY(EditAnywhere, Category = "Combat|Haste")
	TSubclassOf<AASMonsterAttackIndicatorActor> HasteIndicatorClass;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float DetectionRadius = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float LoseTargetRadius = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float AssistAggroRadius = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0"))
	float DirectHitAggro = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0"))
	float AggroDecayPerSecond = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.01", Units = "s"))
	float TargetRefreshInterval = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Navigation", meta = (ClampMin = "0.01", Units = "s"))
	float PathRefreshInterval = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Navigation", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetMoveRepathDistance = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Death", meta = (ClampMin = "0.0", Units = "s"))
	float DeathDespawnDelay = 1.5f;

	void Validate();
};

