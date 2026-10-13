#include "Core/Framework/ASGameMode.h"
#include "Core/Framework/ASGameState.h"
#include "Core/Spawn/ASSpawnSubsystem.h"
#include "Items/Loot/ASLootDaySpawnSubsystem.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "Core/Framework/ASPlayerRecordComponent.h"
#include "Core/Framework/ASRespawnPoint.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
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

bool AASGameMode::IsFinalBossDay(int32 Day) const
{
	if (BossDays.Num() == 0) return false;
	
	int32 LastBossDay = BossDays[0];
	for (int32 BossDay : BossDays)
	{
		LastBossDay = FMath::Max(LastBossDay, BossDay);
	}
	return Day == LastBossDay;
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

	if (UASSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<UASSpawnSubsystem>())
	{
		SpawnSubsystem->HandleDayStarted(Day, DaySpawnDataTable, MonsterSpawnGroupDataTable);
	}
	if (UASLootDaySpawnSubsystem* LootSpawnSubsystem = GetWorld()->GetSubsystem<UASLootDaySpawnSubsystem>())
	{
		LootSpawnSubsystem->HandleDayStarted(Day, LootDaySpawnScheduleDataTable, Duration);
	}
	
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
	if (UASLootDaySpawnSubsystem* LootSpawnSubsystem = GetWorld()->GetSubsystem<UASLootDaySpawnSubsystem>())
	{
		LootSpawnSubsystem->HandleDayEnded();
	}
	
	const int32 Day = GS->GetCurrentDay();
	const float Duration = GetNightDuration(Day);
	const bool bBossNight = IsBossDay(Day);
	
	GS->ResetWaveCounts();
	bBossDefeatedThisNight = false;
	GS->SetPhase(EGamePhase::Night, Duration);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 밤 시작 (%.0f초, 보스: %s)"),
		Day, Duration, bBossNight ? TEXT("O") : TEXT("X"));
	
	// AI/보스 시스템에 밤 시작을 알린다.
	OnNightStarted.Broadcast(Day, bBossNight);

	// 스폰 위치 선정, 기존 몬스터 집계, 웨이브 생성은 SpawnSubsystem이 전담한다.
	if (UASSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<UASSpawnSubsystem>())
	{
		SpawnSubsystem->HandleNightStarted(Day, WaveDataTable);
	}
	
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &AASGameMode::OnNightTimeUp, Duration, false);
}

void AASGameMode::OnNightTimeUp()
{
	AASGameState* GS = GetASGameState();
	if (!GS) return;

	if (UASSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<UASSpawnSubsystem>())
	{
		SpawnSubsystem->HandleNightEnded();
	}
	
	const int32 Day = GS->GetCurrentDay();
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 밤 종료"), Day);
	
	OnNightEnded.Broadcast(Day);
	
	// 웨이브 결과 방송
	const FWaveResult Result = BuildWaveResult();
	OnWaveEnded.Broadcast(Result);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 웨이브 결과: %d/%d 처치 (올클리어: %s)"),
		Result.Day, Result.KilledCount, Result.SpawnedCount, Result.bAllCleared ? TEXT("O") : TEXT("X"));
	
	// 보스 날 밤이 끝날 때까지 보스를 못잡으면 패배
	// 보스를 잡으면 그 다음 시점에 바로 승리 처리
	if (IsBossDay(Day) && !bBossDefeatedThisNight)
	{
		EndGame(false);
		return;
	}
	
	StartDay(Day + 1);
}

void AASGameMode::TryEndNightEarly()
{
	if (IsGameEnded()) return;
	
	AASGameState* GS = GetASGameState();
	if (!GS || GS->GetCurrentPhase() != EGamePhase::Night) return;
	
	// 보스날 밤은 보스까지 잡아야 조기 종료
	if (IsBossDay(GS->GetCurrentDay()) && !bBossDefeatedThisNight) return;
	
	// 웨이브 물량이 아직 다 나오지 않았으면 대기
	const UASSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<UASSpawnSubsystem>();
	if (SpawnSubsystem && !SpawnSubsystem->IsWaveSpawnFinished()) return;
	
	// 스폰 0이면 전멸로 보지 않음
	const int32 Spawned = GS->GetWaveSpawnedCount();
	if (Spawned <= 0 || GS->GetWaveKilledCount() < Spawned) return;
	
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 밤 몬스터 전멸, 밤 조기 종료"), GS->GetCurrentDay());
	
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	OnNightTimeUp();
}

void AASGameMode::EndGame(bool bVictory)
{
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	if (UASSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<UASSpawnSubsystem>())
	{
		SpawnSubsystem->StopAllSpawning();
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
	
	// 집계 후 전멸 여부 확인
	TryEndNightEarly();
}

void AASGameMode::OnBossDefeated()
{
	if (IsGameEnded()) return;
	
	AASGameState* GS = GetASGameState();
	if (!GS) return;
	
	// 보스 날 밤에 잡았을 때만 처리
	const int32 Day = GS->GetCurrentDay();
	if (GS->GetCurrentPhase() != EGamePhase::Night || !IsBossDay(Day)) return;
	
	// 마지막 보스 날이면 승리
	if (IsFinalBossDay(Day))
	{
		UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 최종 보스 처치"), Day);
		EndGame(true);
		return;
	}
	
	// 중간 보스 날이면 표시만 하고, 몬스터 전멸 확인
	bBossDefeatedThisNight = true;
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %d일차 보스 처치"), Day);
	TryEndNightEarly();
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
