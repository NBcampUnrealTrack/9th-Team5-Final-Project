#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"

/**
 * 팀 식별자. 엔진의 FGenericTeamId 에 넣어 쓰는 값이다.
 *
 * 엔진 기본 규칙(같은 팀은 우호, 다른 팀은 적대, NoTeam 은 중립)을 그대로 쓴다.
 * 그래서 플레이어(1)와 몬스터(2)는 서로 적대로 판정되고, 별도 태도 판정 함수를 등록하지 않는다.
 */
namespace ASTeam
{
	inline constexpr uint8 PlayerTeamId = 1;
	inline constexpr uint8 MonsterTeamId = 2;

	inline FGenericTeamId Player() { return FGenericTeamId(PlayerTeamId); }
	inline FGenericTeamId Monster() { return FGenericTeamId(MonsterTeamId); }
}
