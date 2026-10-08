#include "Core/Framework/ASGameMode.h"
#include "Core/Framework/ASGameState.h"
#include "Core/Spawn/ASSpawnSubsystem.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "Core/Framework/ASPlayerRecordComponent.h"
#include "Core/Framework/ASRespawnPoint.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"

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
	
	// 맵의 NavMesh 위에 적 스폰 포인트 자동 생성
	GenerateEnemySpawnPoints();
	
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
	
	// AI/보스 시스템에 밤 시작을 알린다.
	OnNightStarted.Broadcast(Day, bBossNight);

	// 몬스터가 살아 있는 수와 관계없이 목표 물량이 모두 생성될 때까지 스웜을 계속 투입한다.
	if (UASSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<UASSpawnSubsystem>())
	{
		SpawnSubsystem->StartWave(Day, WaveDataTable);
	}
	
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &AASGameMode::OnNightTimeUp, Duration, false);
}

int32 AASGameMode::CountAliveMonsters() const
{
	int32 Count = 0;
	for (TActorIterator<AASMonsterCharacter> It(GetWorld()); It; ++It)
	{
		const AASMonsterCharacter* Monster = *It;
		
		// 죽었지만 제거 전(사망연출)인 몬스터 제외
		if (IsValid(Monster) && !Monster->IsActorBeingDestroyed() && !Monster->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

void AASGameMode::OnNightTimeUp()
{
	AASGameState* GS = GetASGameState();
	if (!GS) return;

	if (UASSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<UASSpawnSubsystem>())
	{
		SpawnSubsystem->StopWave();
	}
	
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
	if (UASSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<UASSpawnSubsystem>())
	{
		SpawnSubsystem->StopWave();
	}
	
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
	
	// 개인 처치 수 (처치자가 플레이어일 때만)
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

// 사망과 리스폰

void AASGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	// 접속한 플레이어에게 기록 컴포넌트 미리 붙이기
	GetOrCreateRecord(NewPlayer);
}

UASPlayerRecordComponent* AASGameMode::GetOrCreateRecord(AController* Controller) const
{
	if (!Controller) return nullptr;
	
	APlayerState* PS = Controller->GetPlayerState<APlayerState>();
	if (!PS) return nullptr;
	
	// 이미 붙어 있으면 그대로 사용
	if (UASPlayerRecordComponent* Record = PS->FindComponentByClass<UASPlayerRecordComponent>())
	{
		return Record;
	}
	
	// 없으면 새로 만들어서 붙이기
	UASPlayerRecordComponent* NewRecord = NewObject<UASPlayerRecordComponent>(PS, TEXT("PlayerRecord"));
	NewRecord->RegisterComponent();
	return NewRecord;
}

void AASGameMode::OnPlayerDied(AController* DeadController)
{
	if (!DeadController || IsGameEnded()) return;
	
	if (IsDeathCountEnabled())
	{
		if (UASPlayerRecordComponent* Record = GetOrCreateRecord(DeadController))
		{
			Record->AddDeath();
			UE_LOG(LogTemp, Log, TEXT("[GameMode] 사망 %d회 / 허용 %d회"), Record->GetDeathCount(), MaxDeathCount);
			
			// 허용 횟수를 넘으면 패배
			if (Record->GetDeathCount() > MaxDeathCount)
			{
				EndGame(false);
				return;
			}
		}
	}
	
	// 대기 시간 후 리스폰
	FTimerHandle RespawnHandle;
	FTimerDelegate RespawnDelegate = FTimerDelegate::CreateUObject(this, &AASGameMode::RespawnPlayer, DeadController);
	GetWorldTimerManager().SetTimer(RespawnHandle, RespawnDelegate, RespawnDelay, false);
}

void AASGameMode::RespawnPlayer(AController* Controller)
{
	if (!Controller || IsGameEnded()) return;
	
	// 기존 Pawn 정리
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}
	
	// 리스폰 포인트 중 랜덤 (선택 UI는 미니맵 이후)
	TArray<AASRespawnPoint*> Points = GetAvailableRespawnPoints();
	if (Points.Num() > 0)
	{
		AASRespawnPoint* Point = Points[FMath::RandRange(0, Points.Num() - 1)];
		// 포인트는 바닥 높이라 캡슐 절반만큼 올려서 생성
		FTransform SpawnTransForm = Point->GetActorTransform();
		SpawnTransForm.AddToTranslation(FVector(0.f, 0.f, GetSpawnHeightOffset(Controller)));
		RestartPlayerAtTransform(Controller, SpawnTransForm);
	}
	else
	{
		// 포인트가 없으면 PlayerStart에서
		RestartPlayer(Controller);
	}
}

TArray<AASRespawnPoint*> AASGameMode::GetAvailableRespawnPoints() const
{
	TArray<AASRespawnPoint*> Result;
	for (TActorIterator<AASRespawnPoint> It(GetWorld()); It; ++It)
	{
		Result.Add(*It);
	}
	return Result;
}

float AASGameMode::GetSpawnHeightOffset(AController* Controller)
{
	// 캡슐 바닥이 지면에 붙으면 충돌로 판정될 수 있어서 여유를 둠
	constexpr float Margin = 10.f;
	constexpr float FallbackHeight = 100.f;
	
	UClass* PawnClass = GetDefaultPawnClassForController(Controller);
	const ACharacter* CharacterCDO = PawnClass ? Cast<ACharacter>(PawnClass->GetDefaultObject()) : nullptr;
	
	if (CharacterCDO && CharacterCDO->GetCapsuleComponent())
	{
		return CharacterCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + Margin;
	}
	
	// 캐릭터가 아니거나 캡슐이 없으면 기본값
	return FallbackHeight;
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

void AASGameMode::DebugKillPlayer()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APlayerState* PS = PC ? PC->GetPlayerState<APlayerState>() : nullptr;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] 플레이어 ASC를 찾지 못해 사망 처리를 할 수 없음"));
		return;
	}
	
	// 체력을 0으로 만들어 실제 사망 과정(사망 태그 -> OnPlayerDied)를 그대로 탐
	// 이미 죽어 있으면 캐릭터 쪽에서 무시
	ASC->SetNumericAttributeBase(UASHealthAttributeSet::GetHealthAttribute(), 0.f);
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

bool AASGameMode::FindSpawnLocation(int32 EnemyCount, TArray<FVector>& OutLocations) const
{
	/* [이전] 플레이어 주변 링에서 랜덤 위치
	const APawn* Target = GetRandomAlivePlayerPawn();
	if (!Target) return false;
	
	return FindSpawnLocationAround(Target->GetActorLocation(),
		SpawnMinDistance, SpawnMaxDistance, OutLocation);
	*/
	
	OutLocations.Reset();
	if (EnemyCount <= 0) return false;
	
	TArray<FVector> Candidates = GetAvailableEnemySpawnPoints();
	if (Candidates.Num() == 0) return false;
	
	// 적 수에 비례해서 사용할 포인트 수 (포인트 1개당 EnemiesPerSpawnPoint마리, 최소1개, 최대 후보수)
	const int32 PointCount = FMath::Clamp(FMath::DivideAndRoundUp(EnemyCount, FMath::Max(1, EnemiesPerSpawnPoint)), 1, Candidates.Num());
	
	// 후보 순서를 섞고 앞에서부터 사용 (겹치지 않게 랜덤 선택)
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		Candidates.Swap(i, FMath::RandRange(0, i));
	}
	
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	
	// 적을 선택된 포인트들에 나눠 배정하고, 포인트 주변에 흩뿌림
	for (int32 i = 0; i < EnemyCount; ++i)
	{
		const FVector& Point = Candidates[i % PointCount];
		FVector Location = Point;
		
		if (NavSys)
		{
			const FVector2D Offset = FMath::RandPointInCircle(SpawnScatterRadius);
			FNavLocation NavLocation;
			if (NavSys->ProjectPointToNavigation(Point + FVector(Offset, 0.f), NavLocation, FVector(100.f, 100.f, 500.f)))
			{
				Location = NavLocation.Location;
			}
		}
		
		OutLocations.Add(Location);
	}
	
	return true;
}

void AASGameMode::GenerateEnemySpawnPoints()
{
	EnemySpawnPoints.Reset();
	
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] NavMesh가 없어 적 스폰 포인트를 만들지 못함"));
		return;
	}
	
	// 간격 조건 때문에 실패할 시 재시도
	const int32 MaxAttempts = SpawnPointCount * 20;
	for (int32 i = 0; i < MaxAttempts && EnemySpawnPoints.Num() < SpawnPointCount; ++i)
	{
		FNavLocation RandomPoint;
		if (!NavSys->GetRandomPoint(RandomPoint)) continue;
		
		// 다른 포인트와 너무 가까우면 버림
		bool bTooCloseToOther = false;
		for (const FVector& Existing : EnemySpawnPoints)
		{
			if (FVector::Dist2D(Existing, RandomPoint.Location) < SpawnPointMinSpacing)
			{
				bTooCloseToOther = true;
				break;
			}
		}
		
		if (!bTooCloseToOther)
		{
			EnemySpawnPoints.Add(RandomPoint.Location);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[GameMode] 적 스폰 포인트 %d개 생성"), EnemySpawnPoints.Num());
}

bool AASGameMode::IsNearAnyPlayer(const FVector& Location) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		
		if (Pawn && FVector::Dist2D(Pawn->GetActorLocation(), Location) < SpawnMinDistance)
		{
			return true;
		}
	}
	return false;
}

TArray<FVector> AASGameMode::GetAvailableEnemySpawnPoints() const
{
	TArray<FVector> Result;
	for (const FVector& Point : EnemySpawnPoints)
	{
		if (!IsNearAnyPlayer(Point))
		{
			Result.Add(Point);
		}
	}
	
	// 전부 플레이어 근처라면 전체 사용
	return Result.Num() > 0 ? Result : EnemySpawnPoints;
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

void AASGameMode::DebugShowEnemySpawnPoints(int32 EnemyCount)
{
	UWorld* World = GetWorld();
	
	// 플레이어 주변 최소 거리 (초록 원)
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (const APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			DrawDebugCircle(World, Pawn->GetActorLocation(), SpawnMinDistance, 64, FColor::Green, false, 10.f, 0, 10.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
		}
	}
	
	// 전체 포인트: 사용 가능 파랑, 제외 빨강
	int32 Available = 0;
	for (const FVector& Point : EnemySpawnPoints)
	{
		const bool bNear = IsNearAnyPlayer(Point);
		DrawDebugSphere(World, Point, 150.f, 12, bNear ? FColor::Red : FColor::Blue, false, 10.f, 0, 5.f);
		if (!bNear) ++Available;
	}
	
	// 이번 요청으로 정해진 적 위치 (노랑)
	TArray<FVector> Locations;
	if (EnemyCount > 0 && FindSpawnLocation(EnemyCount, Locations))
	{
		for (const FVector& Location : Locations)
		{
			DrawDebugSphere(World, Location, 40.f, 8, FColor::Yellow, false, 10.f, 0, 3.f);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Debug] 적 스폰 포인트 %d개 중 사용 가능 %d개, 적 %d마리 위치 표시"), EnemySpawnPoints.Num(), Available, Locations.Num());
}
