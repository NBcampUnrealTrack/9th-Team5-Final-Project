#include "Core/Framework/ASGameState.h"
#include "Net/UnrealNetwork.h"
#include "MapSystem/Zone/Core/ASMapStateComponent.h"

AASGameState::AASGameState()
{
	MapState = CreateDefaultSubobject<UASMapStateComponent>(TEXT("MapState"));
}

void AASGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AASGameState, CurrentPhase);
	DOREPLIFETIME(AASGameState, CurrentDay);
	DOREPLIFETIME(AASGameState, MonsterStatDifficulty);
	DOREPLIFETIME(AASGameState, PhaseEndServerTime);
	DOREPLIFETIME(AASGameState, WaveSpawnedCount);
	DOREPLIFETIME(AASGameState, WaveKilledCount);
	DOREPLIFETIME(AASGameState, TeamRevivesRemaining);
}

void AASGameState::SetPhase(EGamePhase NewPhase, float Duration)
{
	if (!HasAuthority()) return;
	
	CurrentPhase = NewPhase;
	PhaseEndServerTime = (Duration > 0.f) ? GetServerWorldTimeSeconds() + Duration : 0.f;
	
	// 서버에서는 OnRep가 자동 호출되지 않으므로 직접 호출
	OnRep_CurrentPhase();
}

void AASGameState::SetCurrentDay(int32 NewDay)
{
	if (!HasAuthority()) return;
	CurrentDay = NewDay;
}

void AASGameState::SetMonsterStatDifficulty(const float NewDifficulty)
{
	if (!HasAuthority()) return;
	MonsterStatDifficulty = FMath::Max(0.0f, NewDifficulty);
}

void AASGameState::ResetWaveCounts()
{
	if (!HasAuthority()) return;
	WaveSpawnedCount = 0;
	WaveKilledCount = 0;
}

void AASGameState::AddWaveSpawned(int32 Count)
{
	if (!HasAuthority()) return;
	WaveSpawnedCount += Count;
}

void AASGameState::AddWaveKilled()
{
	if (!HasAuthority()) return;
	++WaveKilledCount;
}

void AASGameState::SetTeamRevivesRemaining(int32 NewCount)
{
	if (!HasAuthority()) return;
	TeamRevivesRemaining = FMath::Max(0, NewCount);
	OnRep_TeamRevivesRemaining();
}

bool AASGameState::ConsumeTeamRevive()
{
	if (!HasAuthority() || TeamRevivesRemaining <= 0) return false;
	
	--TeamRevivesRemaining;
	UE_LOG(LogTemp, Log, TEXT("[Revive] 팀 부활 사용, 남은 횟수 %d"), TeamRevivesRemaining);
	OnRep_TeamRevivesRemaining();
	return true;
}

void AASGameState::OnRep_TeamRevivesRemaining()
{
	OnTeamRevivesChanged.Broadcast(TeamRevivesRemaining);
}

float AASGameState::GetPhaseRemainingTime() const
{
	if (PhaseEndServerTime <= 0.f) return 0.f;
	return FMath::Max(0.f, PhaseEndServerTime - GetServerWorldTimeSeconds());
}

void AASGameState::OnRep_CurrentPhase()
{
	OnPhaseChanged.Broadcast(CurrentPhase);
}
