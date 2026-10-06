#include "Core/GameFramework/ASGameMode.h"
#include "Core/GameFramework/ASGameState.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "Core/GameFramework/ASPlayerRecordComponent.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Core/GameFramework/ASRespawnPoint.h"

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
	
	// 웨이브 결과 방송
	const FWaveResult Result = BuildWaveResult();
	OnWaveEnded.Broadcast(Result);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 웨이브 결과: %d/%d 처치 (올클리어: %s)"),
		Result.Day, Result.KilledCount, Result.SpawnedCount, Result.bAllCleared ? TEXT("O") : TEXT("X"));
	
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

// 게임 상태 확인
bool AASGameMode::IsGameEnded() const
{
	const AASGameState* GS = GetASGameState();
	if (!GS) return true;
	
	const EGamePhase Phase = GS->GetCurrentPhase();
	return Phase == EGamePhase::Victory || Phase == EGamePhase::Defeat;
}

FWaveResult AASGameMode::BuildWaveResult() const
{
	FWaveResult Result;
	if (const AASGameState* GS = GetASGameState())
	{
		Result.Day = GS->GetCurrentDay();
		Result.SpawnedCount = GS->GetWaveSpawnedCount();
		Result.KilledCount = GS->GetWaveKilledCount();
		Result.bAllCleared = Result.SpawnedCount > 0 && Result.KilledCount >= Result.SpawnedCount;
	}
	return Result;
}

// AI 쪽에서 호출
void AASGameMode::ReportWaveSpawned(int32 Count)
{
	if (IsGameEnded() || Count <= 0) return;
	
	AASGameState* GS = GetASGameState();
	if (!GS || GS->GetCurrentPhase() != EGamePhase::Night) return;
	
	GS->AddWaveSpawned(Count);
}

void AASGameMode::OnEnemyKilled(AController* Killer)
{
	if (IsGameEnded()) return;
	
	AASGameState* GS = GetASGameState();
	if (!GS) return;
	
	// 웨이브 처치 수는 밤에만 집계
	if (GS->GetCurrentPhase() == EGamePhase::Night)
	{
		GS->AddWaveKilled();
	}
	
	// 처치 수 (PlayerState에 컴포넌트가 붙어 있을 때만)
	if (UASPlayerRecordComponent* Record = GetOrCreateRecord(Killer))
	{
		Record->AddKill();
	}
}

void AASGameMode::OnBossDefeated()
{
	if (IsGameEnded()) return;
	
	AASGameState* GS = GetASGameState();
	if (!GS) return;
	
	// 보스 날 밤에 잡았을 때만 승리
	if (GS->GetCurrentPhase() == EGamePhase::Night && IsBossDay(GS->GetCurrentDay()))
	{
		UE_LOG(LogTemp, Log, TEXT("[GameMode] 보스 처치"));
		EndGame(true);
	}
}

// 디버그

void AASGameMode::DebugSpawnWave(int32 Count)
{
	ReportWaveSpawned(Count);
	if (const AASGameState* GS = GetASGameState())
	{
		UE_LOG(LogTemp, Log, TEXT("[Debug] 스폰 보고: 누적 %d"), GS->GetWaveSpawnedCount());
	}
}

void AASGameMode::DebugKillEnemy()
{
	OnEnemyKilled(nullptr);
	if (const AASGameState* GS = GetASGameState())
	{
		UE_LOG(LogTemp, Log, TEXT("[Debug] 처치: %d/%d"), GS->GetWaveKilledCount(), GS->GetWaveSpawnedCount());
	}
}

void AASGameMode::DebugKillBoss()
{
	OnBossDefeated();
}

// 스폰 위치
APawn* AASGameMode::GetRandomAlivePlayerPawn() const
{
	TArray<APawn*> Pawns;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PC = It->Get())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawns.Add(Pawn);
			}
		}
	}
	
	if (Pawns.Num() == 0) return nullptr;
	return Pawns[FMath::RandRange(0, Pawns.Num() - 1)];
}

bool AASGameMode::FindSpawnLocation(FVector& OutLocation) const
{
	const APawn* Target = GetRandomAlivePlayerPawn();
	if (!Target) return false;
	
	return FindSpawnLocationAround(Target->GetActorLocation(),
		SpawnMinDistance, SpawnMaxDistance, OutLocation);
}

bool AASGameMode::FindSpawnLocationAround(const FVector& Center, float MinDistance, float MaxDistance, FVector& OutLocation) const
{
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return false;
	
	for (int32 i = 0; i < SpawnFindAttempts; ++i)
	{
		// 링 안에서 랜덤 방향, 랜덤 거리
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Distance = FMath::FRandRange(MinDistance, MaxDistance);
		const FVector Candidate = Center + FVector(FMath::Cos(Angle) * Distance,
			FMath::Sin(Angle) * Distance, 0.f);
		
		// 후보 지점을 NavMesh 위로 (위아래 10m 범위)
		FNavLocation NavLocation;
		if (NavSys->ProjectPointToNavigation(Candidate, NavLocation, FVector(200.f, 200.f, 1000.f)))
		{
			// 맞춘 뒤에도 최소 거리 재확인
			if (FVector::Dist2D(NavLocation.Location, Center) >= MinDistance)
			{
				OutLocation = NavLocation.Location;
				return true;
			}
		}
	}
	
	return false;
}

void AASGameMode::DebugShowSpawnPoints(int32 Count)
{
	const APawn* Target = GetRandomAlivePlayerPawn();
	if (!Target) return;
	
	const FVector Center = Target->GetActorLocation();
	UWorld* World = GetWorld();
	
	// 초록: 최소 거리, 파랑 : 최대 거리
	DrawDebugCircle(World, Center, SpawnMinDistance, 64, FColor::Green,
		false, 10.f, 0, 10.f,
		FVector(1, 0, 0), FVector(0, 1, 0), false);
	
	DrawDebugCircle(World, Center, SpawnMaxDistance, 64, FColor::Blue,
		false, 10.f, 0, 10.f,
		FVector(1, 0, 0), FVector(0, 1, 0), false);
	
	int32 Found = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		FVector Location;
		if (FindSpawnLocationAround(Center, SpawnMinDistance, SpawnMaxDistance, Location))
		{
			DrawDebugSphere(World, Location, 80.f, 12, FColor::Red,
				false, 10.f, 0, 5.f);
			
			++Found;
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Debug] 스폰 위치 %d/%d개 찾음"), Found, Count);
}

// 플레이어 기록
void AASGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	GetOrCreateRecord(NewPlayer);
}

UASPlayerRecordComponent* AASGameMode::GetOrCreateRecord(AController* Controller) const
{
	if (!Controller || !Controller->PlayerState) return nullptr;
	
	APlayerState* PS = Controller->PlayerState;
	if (UASPlayerRecordComponent* Existing = PS->FindComponentByClass<UASPlayerRecordComponent>())
	{
		return Existing;
	}
	
	// 캐릭터 쪽 PlayerState BP에 붙어 있지 않으면 코드로 붙임
	UASPlayerRecordComponent* NewRecord = NewObject<UASPlayerRecordComponent>(PS, TEXT("PlayerRecord"));
	NewRecord->RegisterComponent();
	return NewRecord;
}

// 사망과 리스폰
void AASGameMode::OnPlayerDied(AController* DeadController)
{
	if (IsGameEnded() || !DeadController) return;
	
	if (IsDeathCountEnabled())
	{
		UASPlayerRecordComponent* Record = GetOrCreateRecord(DeadController);
		if (!Record) return;
		
		Record->AddDeath();
		UE_LOG(LogTemp, Log, TEXT("[GameMode] 플레이어 사망 (데스카운트 %d / %d)"),
			Record->GetDeathCount(), MaxDeathCount);
		
		if (Record->GetDeathCount() > MaxDeathCount)
		{
			EndGame(false);
			return;
		}
	}
	// TODO 멀티는 기절 부활 방식
	
	// 일정 시간 뒤 리스폰
	FTimerHandle RespawnHandle;
	FTimerDelegate RespawnDelegate = FTimerDelegate::CreateUObject(this,
		&AASGameMode::RespawnPlayer, DeadController);
	GetWorldTimerManager().SetTimer(RespawnHandle, RespawnDelegate, RespawnDelay, false);
}

void AASGameMode::RespawnPlayer(AController* Controller)
{
	if (IsGameEnded() || !IsValid(Controller)) return;
	
	// 시체 처리
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}
	
	const TArray<AASRespawnPoint*> Points = GetAvailableRespawnPoints();
	if (Points.Num() > 0)
	{
		const AASRespawnPoint* Point = Points[FMath::RandRange(0, Points.Num() -1)];
		RestartPlayerAtTransform(Controller, Point->GetActorTransform());
		UE_LOG(LogTemp, Log, TEXT("[GameMode] 리스폰: %s"), *Point->DisplayName.ToString());
	}
	else
	{
		// 리스폰 포인트가 없으면 PlayerStart에서
		RestartPlayer(Controller);
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 리스폰 포인트 없음, PlayerStart에서 리스폰"));
	}
}

TArray<AASRespawnPoint*> AASGameMode::GetAvailableRespawnPoints() const
{
	// TODO: 구역 시스템이 생기면 맵 쪽 조회 함수로 교체
	TArray<AASRespawnPoint*> Points;
	for (TActorIterator<AASRespawnPoint> It(GetWorld()); It; ++It)
	{
		Points.Add(*It);
	}
	return Points;
}

void AASGameMode::DebugKillPlayer()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		OnPlayerDied(PC);
	}
}
