#include "Core/GameFramework/ASGameMode.h"
#include "Core/GameFramework/ASGameState.h"
#include "TimerManager.h"

AASGameMode::AASGameMode()
{
	GameStateClass = AASGameState::StaticClass();
}

// 규칙 조회
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

AASGameState* AASGameMode::GetASGameState() const
{
	return GetGameState<AASGameState>();
}

// 단계 진행

void AASGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	
	AASGameState* GS = GetASGameState();
	if (!GS) return;
	
	GS->SetPhase(EGamePhase::Ready, ReadyDuration);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] Ready (%.0f초)"), ReadyDuration);
	
	// Ready가 끝나면 1일차 시작
	FTimerDelegate StartDelegate = FTimerDelegate::CreateUObject(this, &AASGameMode::StartDay, 1);
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, StartDelegate, ReadyDuration, false);
}

void AASGameMode::StartDay(int32 Day)
{
	AASGameState* GS = GetASGameState();
	if (!GS) return;
	
	const float Duration = GetDayDuration(Day);
	GS->SetCurrentDay(Day);
	GS->SetPhase(EGamePhase::Day, Duration);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 낮 시작 (%.0f초)"), Day, Duration);
	
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &AASGameMode::OnDayTimeUp, Duration, false);
}

void AASGameMode::OnDayTimeUp()
{
	StartNight();
}

void AASGameMode::StartNight()
{
	AASGameState* GS = GetASGameState();
	if (!GS) return;
	
	const int32 Day = GS->GetCurrentDay();
	const float Duration = GetNightDuration(Day);
	const bool bBossNight = IsBossDay(Day);
	
	GS->ResetWaveCounts();
	GS->SetPhase(EGamePhase::Night, Duration);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 밤 시작 (%.0f초, 보스: %s)"),
		Day, Duration, bBossNight ? TEXT("O") : TEXT("X"));
	
	// AI 쪽에 알림 (스폰 수와 보스 스폰은 AI 쪽에서 결정)
	OnNightStarted.Broadcast(Day, bBossNight);
	
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &AASGameMode::OnNightTimeUp, Duration, false);
}

void AASGameMode::OnNightTimeUp()
{
	AASGameState* GS = GetASGameState();
	if (!GS) return;
	
	const int32 Day = GS->GetCurrentDay();
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 밤 종료"), Day);
	
	OnNightEnded.Broadcast(Day);
	
	// 보스 밤이 끝날 때까지 보스를 못잡으면 패배
	// 보스를 잡으면 그 다음 시점에 바로 승리 처리
	if (IsBossDay(Day))
	{
		EndGame(false);
		return;
	}
	
	StartDay(Day + 1);
}

void AASGameMode::EndGame(bool bVictory)
{
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	
	if (AASGameState* GS = GetASGameState())
	{
		GS->SetPhase(bVictory ? EGamePhase::Victory : EGamePhase::Defeat, 0.f);
	}
	UE_LOG(LogTemp, Log, TEXT("[GameMode] 게임 종료: %s"), bVictory ? TEXT("승리") : TEXT("패배"));
}

// 디버그

void AASGameMode::DebugSkipPhase()
{
	AASGameState* GS = GetASGameState();
	if (!GS) return;
	
	// 현재 단계에 맞는 "시간 종료" 처리를 바로 실행
	switch (GS->GetCurrentPhase())
	{
	case EGamePhase::Ready:
		StartDay(1);
		break;
	case EGamePhase::Day:
		OnDayTimeUp();
		break;
	case EGamePhase::Night:
		OnNightTimeUp();
		break;
	default:
		break;	// Victory, Defeat에서는 아무것도 하지 않음
	}
}
