#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassRepresentationTypes.h"
#include "Core/Combat/ASCombatImpactTypes.h"
#include "Mass/Monster/Data/ASMassMonsterTypes.h"
#include "ASMassMonsterTemporaryVisualFragment.generated.h"

/** 임시 색상 표현에서 사용할 우선순위 상태. */
UENUM()
enum class EASMassMonsterTemporaryVisualState : uint8
{
	DaySpawn,
	NightSpawn,
	Hit,
	Dead
};

/**
 * 임시 Mass 몬스터 색상 표현 전용 런타임 데이터.
 * 스폰 당시의 행동 유형을 보관해, 낮/밤 전환으로 WorldBehavior가 변경되어도 기본 색상이 바뀌지 않는다.
 */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterTemporaryVisualFragment : public FMassFragment
{
	GENERATED_BODY()

	EASMassMonsterWorldBehavior SpawnWorldBehavior = EASMassMonsterWorldBehavior::DayTerritory;
	float HitFlashTimeRemaining = 0.0f;
	EASResolvedHitReaction LastImpactReaction = EASResolvedHitReaction::VisualOnly;
	uint8 LastImpactReactionSequence = 0;
	EASMassMonsterTemporaryVisualState AppliedState = EASMassMonsterTemporaryVisualState::DaySpawn;
	bool bHasAppliedState = false;

	/** Entity Config의 원본 Descriptor. 상태 표현 종료 후 이 외형으로 돌아간다. */
	FStaticMeshInstanceVisualizationDescHandle BaseVisualHandle;
};
