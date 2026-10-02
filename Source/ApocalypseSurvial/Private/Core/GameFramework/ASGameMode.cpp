#include "Core/GameFramework/ASGameMode.h"
#include "Core/GameFramework/ASGameState.h"

AASGameMode::AASGameMode()
{
	GameStateClass = AASGameState::StaticClass();
}

float AASGameMode::GetDayDuration(int32 Day) const
{
	return DayDuration;
}

float AASGameMode::GetNightDuration(int32 Day) const
{
	return NightDuration;
}

bool AASGameMode::IsBossDay(int32 Day) const
{
	return BossDays.Contains(Day);
}

bool AASGameMode::IsDeathCountEnabled() const
{
	// 프로토 타입용 항상 사용
	// TODO : 멀티 생기면 선택값으로 판단
	return true;
}
