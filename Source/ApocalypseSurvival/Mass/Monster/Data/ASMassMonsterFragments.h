#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/Monster/Data/ASMassMonsterTypes.h"
#include "ASMassMonsterFragments.generated.h"

/** Entity마다 독립적으로 보유하는 현재 체력. 최대 체력은 종류별 공유 설정에 둔다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterHealthFragment : public FMassFragment
{
	GENERATED_BODY()

	float CurrentHealth = 100.0f;
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
	EASMassMonsterAttackState AttackState = EASMassMonsterAttackState::Ready;
};

/** 낮 귀환의 기준이 되는 개체별 홈 위치와 추격 제한. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterHomeFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector HomeLocation = FVector::ZeroVector;
	float LeashRadius = 2000.0f;
	float ReturnAcceptanceRadius = 100.0f;
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

/** 같은 종류의 일반 몬스터가 공유하는 읽기 전용 설정. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Identity")
	FName MonsterTypeId = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackPower = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.01", Units = "s"))
	float AttackCooldown = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	EASMassMonsterAttackPattern AttackPattern = EASMassMonsterAttackPattern::Melee;

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

