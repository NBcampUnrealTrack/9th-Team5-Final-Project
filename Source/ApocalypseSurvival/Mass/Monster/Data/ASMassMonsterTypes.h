#pragma once

#include "CoreMinimal.h"
#include "ASMassMonsterTypes.generated.h"

/** 일반 Mass 몬스터의 상위 행동 상태. 자주 바뀌므로 Tag가 아닌 Fragment 값으로 관리한다. */
UENUM(BlueprintType)
enum class EASMassMonsterState : uint8
{
	Idle,
	Chase,
	Attack,
	ReturnHome,
	Dead
};

/** 낮의 영역 방어와 밤의 전역 공격 규칙을 구분한다. */
UENUM(BlueprintType)
enum class EASMassMonsterWorldBehavior : uint8
{
	DayTerritory,
	NightAssault
};

/** 일반 몬스터 한 종류가 보유하는 단일 핵심 공격 패턴. */
UENUM(BlueprintType)
enum class EASMassMonsterAttackPattern : uint8
{
	Melee,
	RangedProjectile,
	Suicide,
	Charge
};

/** 공격 준비와 실행 결과를 Processor 사이에서 공유하는 런타임 상태. */
UENUM()
enum class EASMassMonsterAttackState : uint8
{
	Ready,
	Windup,
	Recovering
};

