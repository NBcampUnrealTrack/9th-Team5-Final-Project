#include "Core/Framework/ASGameMode.h"
#include "Core/Framework/ASGameState.h"
#include "Core/Spawn/ASSpawnSubsystem.h"
#include "Core/Framework/ASPlayerRecordComponent.h"
#include "Core/Framework/ASRespawnPoint.h"
#include "Core/Framework/ASCharacterSelectSubsystem.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Core/Framework/ASPlayerState.h"
#include "Core/Framework/ASPlayerController.h"
#include "Items/Loot/ASLootDaySpawnSubsystem.h"
#include "Items/Loot/ASLootChest.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemGrant.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/PlayerStartPIE.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "MapSystem/Zone/Manager/ASMapGameFlowComponent.h"
#include "MapSystem/Vision/ASVisionSetupComponent.h"
#include "MapSystem/Zone/Effects/ASZoneEffectComponent.h"

AASGameMode::AASGameMode()
{
	GameStateClass = AASGameState::StaticClass();
	MapFlow = CreateDefaultSubobject<UASMapGameFlowComponent>(TEXT("MapFlow"));
	VisionSetup = CreateDefaultSubobject<UASVisionSetupComponent>(TEXT("VisionSetup"));
	ZoneEffects = CreateDefaultSubobject<UASZoneEffectComponent>(TEXT("ZoneEffects"));
}

bool AASGameMode::ReturnToLobby()
{
	if (!HasAuthority() || !IsGameEnded() || LobbyMapPath.IsEmpty()) return false;
	const bool bPreviousSeamlessTravel = bUseSeamlessTravel;
	bUseSeamlessTravel = true;
	if (!GetWorld()->ServerTravel(LobbyMapPath + TEXT("?listen"), true))
	{
		bUseSeamlessTravel = bPreviousSeamlessTravel;
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[Session] Returning all players to the existing lobby."));
	return true;
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
	// 싱글은 사망 횟수 제한, 멀티는 쓰러짐/부활 규칙이라 사용하지 않는다.
	return !IsMultiplayerRules();
}

bool AASGameMode::IsDownedEnabled() const
{
	const AASGameState* GS = GetASGameState();
	return IsMultiplayerRules() && GS && GS->GetTeamRevivesRemaining() > 0;
}

bool AASGameMode::IsMultiplayerRules() const
{
	return GetNetMode() != NM_Standalone;
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
	
	// Ready 이후 생성되는 Actor와 Mass가 같은 난이도 입력을 읽도록 GameState에 먼저 복제한다.
	GS->SetMonsterStatDifficulty(MonsterStatDifficulty);
	// 멀티면 팀 공용 부활 횟수를 채운다. 싱글은 0 (쓰러짐 없음)
	GS->SetTeamRevivesRemaining(IsMultiplayerRules() ? TeamReviveCount : 0);
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
		SpawnSubsystem->HandleNightStarted(Day, WaveDataTable, MonsterSpawnGroupDataTable);
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
	OnEnemiesKilled(1, Killer);
}

void AASGameMode::OnEnemiesKilled(const int32 Count, AController* Killer)
{
	if (IsGameEnded() || Count <= 0) return;
	
	AASGameState* GS = GetASGameState();
	if (!GS) return;
	
	// 웨이브 처치 수는 밤에만 집계
	if (GS->GetCurrentPhase() == EGamePhase::Night)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			GS->AddWaveKilled();
		}
	}
	
	// 개인 처치 수 (처치자가 플레이어일 때만)
	if (UASPlayerRecordComponent* Record = GetOrCreateRecord(Killer))
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Record->AddKill();
		}
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

FString AASGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!Error.IsEmpty()) return Error;
	const UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>();
	AASPlayerState* PlayerState = NewPlayerController ? NewPlayerController->GetPlayerState<AASPlayerState>() : nullptr;
	const UASCharacterDefinition* Definition = Sessions ? Sessions->FindMatchCharacter(UniqueId) : nullptr;
	if (PlayerState && Definition)
	{
		PlayerState->SetCharacterDefinition(Definition);
		PlayerState->MarkCharacterSelectionReceived();
	}
	return Error;
}

UClass* AASGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	AASPlayerState* PlayerState = InController ? InController->GetPlayerState<AASPlayerState>() : nullptr;
	if (PlayerState)
	{
		// 리스폰: 이미 정해진 캐릭터로 다시 스폰
		const UASCharacterDefinition* Definition = PlayerState->GetCharacterDefinition();
		
		// 로비를 거치지 않은 로컬 실행은 기존 선택을 사용한다.
		if (!Definition && InController->IsLocalController())
		{
			const UGameInstance* GameInstance = GetGameInstance();
			const UASCharacterSelectSubsystem* SelectSubsystem = GameInstance ? GameInstance->GetSubsystem<UASCharacterSelectSubsystem>() : nullptr;
			Definition = SelectSubsystem ? SelectSubsystem->GetSelectedCharacter() : nullptr;
			
			if (Definition)
			{
				PlayerState->SetCharacterDefinition(Definition);
			}
		}
		
		if (Definition && Definition->CharacterClass)
		{
			return Definition->CharacterClass;
		}
	}
	
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

// 사망과 리스폰
void AASGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	// 접속한 플레이어에게 기록 컴포넌트 미리 붙이기
	GetOrCreateRecord(NewPlayer);
	
	// 원격 클라이언트: 캐릭터 선택이 끝내 오지 않으면 기본 캐릭터로 진행
	if (NewPlayer && !NewPlayer->IsLocalController())
	{
		TWeakObjectPtr<APlayerController> WeakPlayer = NewPlayer;
		FTimerHandle SelectTimeoutHandle;
		FTimerDelegate SelectTimeoutDelegate = FTimerDelegate::CreateWeakLambda(this, [this, WeakPlayer]()
		{
			APlayerController* PlayerController = WeakPlayer.Get();
			AASPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AASPlayerState>() : nullptr;
			if (!PlayerState || PlayerState->HasReceivedCharacterSelection())
			{
				return;
			}
			
			UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] %s 선택이 오지 않아 기본 캐릭터로 스폰"), *GetNameSafe(PlayerController));
			PlayerState->MarkCharacterSelectionReceived();
			NotifyCharacterSelectionReceived(PlayerController);
		});
		GetWorldTimerManager().SetTimer(SelectTimeoutHandle, SelectTimeoutDelegate, CharacterSelectTimeout, false);
	}
}

bool AASGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// 원격 클라이언트는 캐릭터 선택을 받기 전까지 스폰 보류
	if (Player && !Player->IsLocalController())
	{
		const AASPlayerState* PlayerState = Player->GetPlayerState<AASPlayerState>();
		if (PlayerState && !PlayerState->HasReceivedCharacterSelection())
		{
			return false;
		}
	}
	
	return Super::PlayerCanRestart_Implementation(Player);
}

void AASGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending())
	{
		return;
	}

	FTransform SpawnTransform;
	bool bUsedAdjustedLocation = false;
	if (APlayerStart* PlayerStart = FindAvailablePlayerStart(NewPlayer, SpawnTransform, bUsedAdjustedLocation))
	{
		UE_LOG(LogTemp, Log,
			TEXT("[PlayerSpawn] Controller=%s PlayerId=%d Start=%s Adjusted=%d Location=%s"),
			*GetNameSafe(NewPlayer),
			NewPlayer->GetPlayerState<APlayerState>() ? NewPlayer->GetPlayerState<APlayerState>()->GetPlayerId() : INDEX_NONE,
			*GetNameSafe(PlayerStart), bUsedAdjustedLocation, *SpawnTransform.GetLocation().ToString());

		if (!bUsedAdjustedLocation)
		{
			Super::RestartPlayerAtPlayerStart(NewPlayer, PlayerStart);
		}
		else
		{
			Super::RestartPlayerAtTransform(NewPlayer, SpawnTransform);
			if (NewPlayer->GetPawn())
			{
				NewPlayer->StartSpot = PlayerStart;
				InitStartSpot(PlayerStart, NewPlayer);
			}
		}
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[PlayerSpawn] No usable regular PlayerStart for %s; falling back to the engine spawn path."),
		*GetNameSafe(NewPlayer));
	Super::RestartPlayer(NewPlayer);
}

APlayerStart* AASGameMode::FindAvailablePlayerStart(AController* Controller, FTransform& OutSpawnTransform,
	bool& bOutUsedAdjustedLocation)
{
	bOutUsedAdjustedLocation = false;
	UWorld* World = GetWorld();
	UClass* PawnClass = GetDefaultPawnClassForController(Controller);
	APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
	if (!World || !PawnToFit)
	{
		return nullptr;
	}

	TArray<APlayerStart*> PlayerStarts;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* PlayerStart = *It;
		if (PlayerStart && !PlayerStart->IsA<APlayerStartPIE>())
		{
			PlayerStarts.Add(PlayerStart);
		}
	}

	PlayerStarts.Sort([](const APlayerStart& Left, const APlayerStart& Right)
	{
		return Left.GetName() < Right.GetName();
	});
	if (PlayerStarts.IsEmpty())
	{
		return nullptr;
	}

	const int32 StartIndex = GetPlayerStartIndex(Controller, PlayerStarts.Num());
	for (int32 Offset = 0; Offset < PlayerStarts.Num(); ++Offset)
	{
		APlayerStart* PlayerStart = PlayerStarts[(StartIndex + Offset) % PlayerStarts.Num()];
		const FVector Location = PlayerStart->GetActorLocation();
		const FRotator Rotation = PlayerStart->GetActorRotation();
		if (!World->EncroachingBlockingGeometry(PawnToFit, Location, Rotation))
		{
			OutSpawnTransform = FTransform(Rotation, Location);
			return PlayerStart;
		}
	}

	for (int32 Offset = 0; Offset < PlayerStarts.Num(); ++Offset)
	{
		APlayerStart* PlayerStart = PlayerStarts[(StartIndex + Offset) % PlayerStarts.Num()];
		FVector AdjustedLocation = PlayerStart->GetActorLocation();
		const FRotator Rotation = PlayerStart->GetActorRotation();
		if (World->FindTeleportSpot(PawnToFit, AdjustedLocation, Rotation))
		{
			OutSpawnTransform = FTransform(Rotation, AdjustedLocation);
			bOutUsedAdjustedLocation = true;
			return PlayerStart;
		}
	}

	return nullptr;
}

int32 AASGameMode::GetPlayerStartIndex(const AController* Controller, const int32 StartCount)
{
	if (StartCount <= 0)
	{
		return 0;
	}

	if (const APlayerState* PlayerState = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr)
	{
		if (PlayerState->GetPlayerId() >= 0)
		{
			return PlayerState->GetPlayerId() % StartCount;
		}
	}

	int32 ConnectionIndex = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++ConnectionIndex)
	{
		if (It->Get() == Controller)
		{
			return ConnectionIndex % StartCount;
		}
	}

	return 0;
}

void AASGameMode::NotifyCharacterSelectionReceived(APlayerController* PlayerController)
{
	if (!PlayerController || PlayerController->GetPawn() || IsGameEnded())
	{
		return;
	}
	
	// 매치 시작 전이면 HandleMatchHasStarted가 스폰한다.
	if (PlayerCanRestart(PlayerController))
	{
		RestartPlayer(PlayerController);
	}
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
	
	// 멀티: 쓰러진 뒤 부활 못 받고 죽으면 그 판은 리스폰 없음 (관전)
	if (IsMultiplayerRules())
	{
		UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 사망 (멀티: 리스폰 없음, 관전)"), *GetNameSafe(DeadController));
		
		APawn* DeadPawn = DeadController->GetPawn();
		
		// 이 플레이어를 보고 있던 관전자들은 다른 파티원으로 옮긴다.
		ReassignSpectatorsWatching(DeadPawn);
		
		CheckPartyWipe();
		
		// 전멸이 아니면 죽은 플레이어도 관전 시작
		if (APlayerController* DeadPlayer = Cast<APlayerController>(DeadController); DeadPlayer && !IsGameEnded())
		{
			StartSpectating(DeadPlayer);
		}
		
		// 시체는 파밍할 수 있게 남기고, 사망자 인벤토리를 시체 루팅 상자로 옮긴다.
		// 파밍이 끝나 상자가 비면 시체를 숨긴다. 옮길 게 없으면 바로 숨긴다.
		if (AASPlayerCharacter* Corpse = Cast<AASPlayerCharacter>(DeadPawn))
		{
			SpawnCorpseLoot(Corpse, DeadController);
		}
		return;
	}
	
	// 대기 시간 후 리스폰
	// 대기 중 컨트롤러가 사라질 수 있으므로(접속 종료 등) 약한 참조로 잡아 둔다.
	TWeakObjectPtr<AController> WeakController = DeadController;
	FTimerHandle RespawnHandle;
	FTimerDelegate RespawnDelegate = FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
	{
		if (AController* Controller = WeakController.Get())
		{
			RespawnPlayer(Controller);
		}
	});
	GetWorldTimerManager().SetTimer(RespawnHandle, RespawnDelegate, RespawnDelay, false);
}

void AASGameMode::Logout(AController* Exiting)
{
	// 멀티 진행 중 퇴장: 엔진 처리(폰 정리) 전에, 폰이 아직 있을 때 관전·전멸을 정리한다.
	if (Exiting && IsMultiplayerRules() && !IsGameEnded())
	{
		UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 퇴장"), *GetNameSafe(Exiting));

		// 판정 함수들이 퇴장자를 건너뛰도록 표시
		LeavingController = Exiting;

		// 퇴장자가 관전 중이었으면 목록에서 제거 (이미 사라진 항목도 함께 정리)
		const APlayerController* ExitingPlayer = Cast<APlayerController>(Exiting);
		Spectators.RemoveAll([ExitingPlayer](const TWeakObjectPtr<APlayerController>& Spectator)
		{
			return !Spectator.IsValid() || Spectator.Get() == ExitingPlayer;
		});

		// 퇴장자를 보던 관전자는 다른 파티원으로 옮긴다
		ReassignSpectatorsWatching(Exiting->GetPawn());

		// 남은 인원 기준으로 전멸 재판정 (서 있는 사람이 없으면 패배)
		CheckPartyWipe();

		LeavingController.Reset();
	}

	Super::Logout(Exiting);
}

void AASGameMode::OnPlayerDowned(AController* DownedController)
{
	if (!DownedController || IsGameEnded()) return;
	
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 쓰러짐"), *GetNameSafe(DownedController));
	CheckPartyWipe();
}

void AASGameMode::CheckPartyWipe()
{
	if (IsGameEnded()) return;
	
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (!PlayerController || PlayerController == LeavingController.Get())
		{
			continue;
		}
		
		// 아직 스폰 전(캐릭터 선택 대기)이면 살아 있는 것으로 본다.
		const AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(PlayerController->GetPawn());
		if (!Character || (!Character->IsDead() && !Character->IsDowned()))
		{
			return;
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[GameMode] 파티 전멸"));
	EndGame(false);
}

TArray<AASPlayerCharacter*> AASGameMode::GetSpectateCandidates(const APlayerController* Spectator) const
{
	TArray<AASPlayerCharacter*> Candidates;
	
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (!PlayerController || PlayerController == Spectator || PlayerController == LeavingController.Get())
		{
			continue;
		}
		
		AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(PlayerController->GetPawn());
		if (Character && !Character->IsDead())
		{
			Candidates.Add(Character);
		}
	}
	
	return Candidates;
}

void AASGameMode::StartSpectating(APlayerController* Spectator)
{
	if (!Spectator)
	{
		return;
	}
	
	const TArray<AASPlayerCharacter*> Candidates = GetSpectateCandidates(Spectator);
	if (Candidates.Num() == 0)
	{
		return;
	}
	
	// 지금 보고 있는 곳(처음엔 자기 시체)에서 가장 가까운 파티원
	const AActor* CurrentView = Spectator->GetViewTarget();
	const FVector From = CurrentView ? CurrentView->GetActorLocation() : FVector::ZeroVector;
	
	AASPlayerCharacter* Nearest = nullptr;
	float NearestDistanceSq = TNumericLimits<float>::Max();
	for (AASPlayerCharacter* Candidate : Candidates)
	{
		const float DistanceSq = FVector::DistSquared(From, Candidate->GetActorLocation());
		if (DistanceSq < NearestDistanceSq)
		{
			NearestDistanceSq = DistanceSq;
			Nearest = Candidate;
		}
	}
	
	Spectators.AddUnique(Spectator);
	
	// 서버에서 바꾸면 해당 클라이언트의 카메라에도 전달된다.
	Spectator->SetViewTargetWithBlend(Nearest, 0.5f);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 관전: %s"), *GetNameSafe(Spectator), *GetNameSafe(Nearest));
}

void AASGameMode::ReassignSpectatorsWatching(const AActor* LostTarget)
{
	if (!LostTarget)
	{
		return;
	}
	
	// StartSpectating 이 목록을 건드릴 수 있어서 복사본으로 돈다.
	const TArray<TWeakObjectPtr<APlayerController>> SpectatorsCopy = Spectators;
	for (const TWeakObjectPtr<APlayerController>& WeakSpectator : SpectatorsCopy)
	{
		APlayerController* Spectator = WeakSpectator.Get();
		if (Spectator && Spectator->GetViewTarget() == LostTarget)
		{
			StartSpectating(Spectator);
		}
	}
}

void AASGameMode::SpectateNext(APlayerController* Spectator)
{
	if (!Spectator || !Spectators.Contains(Spectator) || IsGameEnded())
	{
		return;
	}
	
	const TArray<AASPlayerCharacter*> Candidates = GetSpectateCandidates(Spectator);
	if (Candidates.Num() == 0)
	{
		return;
	}
	
	// 지금 보는 파티원의 다음 순서. 목록에 없으면(이미 죽었으면) 첫 번째
	const int32 CurrentIndex = Candidates.IndexOfByKey(Spectator->GetViewTarget());
	const int32 NextIndex = (CurrentIndex == INDEX_NONE) ? 0 : (CurrentIndex + 1) % Candidates.Num();
	
	Spectator->SetViewTargetWithBlend(Candidates[NextIndex], 0.3f);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 관전 전환: %s"), *GetNameSafe(Spectator), *GetNameSafe(Candidates[NextIndex]));
}

void AASGameMode::KillUnrevivablePlayers()
{
	if (!IsMultiplayerRules() || IsGameEnded())
	{
		return;
	}
	
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		AASPlayerCharacter* Character = PlayerController ? Cast<AASPlayerCharacter>(PlayerController->GetPawn()) : nullptr;
		if (Character && Character->IsDowned())
		{
			UE_LOG(LogTemp, Log, TEXT("[Revive] 팀 부활 횟수 소진, %s 즉시 사망"), *GetNameSafe(Character));
			
			// 시간 초과 사망과 같은 경로 (사망 → 관전 → 시체 파밍)
			Character->BleedOut();
		}
	}
}

void AASGameMode::SpawnCorpseLoot(AASPlayerCharacter* Corpse, AController* DeadController)
{
	if (!IsValid(Corpse))
	{
		return;
	}
	
	const AASPlayerController* DeadPlayer = Cast<AASPlayerController>(DeadController);
	UASInventoryComponent* DeadInventory = DeadPlayer ? DeadPlayer->GetInventoryComponent() : nullptr;
	
	// 장착 무기도 인벤토리 아이템으로 넘기기 위해 먼저 해제한다.
	if (DeadInventory)
	{
		DeadInventory->UnequipItem();
	}
	
	// 상자에 넣을 목록 (정의 + 수량)
	TArray<FASItemGrant> Grants;
	TArray<UASItemInstance*> ItemsToRemove;
	if (DeadInventory)
	{
		for (UASItemInstance* Item : DeadInventory->GetItemInstances())
		{
			if (IsValid(Item) && Item->GetItemDefinition() && Item->GetQuantity() > 0)
			{
				FASItemGrant Grant;
				Grant.ItemDefinition = Item->GetItemDefinition();
				Grant.Quantity = Item->GetQuantity();
				Grants.Add(Grant);
				ItemsToRemove.Add(Item);
			}
		}
	}
	
	// 옮길 게 없거나 상자 클래스가 없으면 시체만 숨긴다.
	if (Grants.Num() == 0 || !CorpseLootClass)
	{
		if (!CorpseLootClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameMode] CorpseLootClass 가 비어 있어 시체 파밍 없이 숨긴다. BP_GameMode 에 BP_CorpseLoot 를 지정해야 한다."));
		}
		Corpse->MulticastHideCorpse();
		return;
	}
	
	// 시체 위치에 상자 생성. Owner 를 시체로 둬서 상자가 사라질 때 어느 시체인지 안다.
	const FTransform SpawnTransform(Corpse->GetActorRotation(), Corpse->GetActorLocation());
	AASLootChest* Loot = GetWorld()->SpawnActorDeferred<AASLootChest>(
		CorpseLootClass, SpawnTransform, Corpse, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Loot)
	{
		Corpse->MulticastHideCorpse();
		return;
	}
	
	Loot->SetInitialLoot(Grants);
	// 루팅 창 제목: "닉네임의 시체"
	const APlayerState* DeadPlayerState = DeadController ? DeadController->GetPlayerState<APlayerState>() : nullptr;
	const FString DeadName = DeadPlayerState ? DeadPlayerState->GetPlayerName() : TEXT("플레이어");
	Loot->SetLootDisplayName(FText::FromString(FString::Printf(TEXT("%s의 시체"), *DeadName)));
	Loot->FinishSpawning(SpawnTransform);
	
	// 내용물 생성에 실패하면 상자는 스스로 사라진다. 그때는 아이템을 지우지 않는다.
	if (!IsValid(Loot) || Loot->IsActorBeingDestroyed())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 시체 루팅 상자 생성 실패, 시체만 숨긴다."));
		Corpse->MulticastHideCorpse();
		return;
	}
	
	// 상자로 옮겼으니 사망자 인벤토리에서는 지운다.
	for (UASItemInstance* Item : ItemsToRemove)
	{
		if (IsValid(Item))
		{
			DeadInventory->RemoveItemInstance(Item, Item->GetQuantity());
		}
	}
	
	Loot->OnDestroyed.AddDynamic(this, &AASGameMode::HandleCorpseLootDestroyed);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 시체 루팅 상자 생성 (아이템 %d종)"), *GetNameSafe(Corpse), Grants.Num());
}

void AASGameMode::HandleCorpseLootDestroyed(AActor* DestroyedActor)
{
	if (AASPlayerCharacter* Corpse = DestroyedActor ? Cast<AASPlayerCharacter>(DestroyedActor->GetOwner()) : nullptr)
	{
		Corpse->MulticastHideCorpse();
	}
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

	// RestartPlayer 계열은 PossessedBy까지 동기적으로 완료한다. 따라서 PlayerState ASC의
	// Avatar가 새 Pawn으로 연결된 뒤에 보호 GE를 적용할 수 있다.
	ApplyRespawnInvulnerability(Controller);
}

void AASGameMode::ApplyRespawnInvulnerability(AController* Controller) const
{
	if (!Controller || !Controller->GetPawn() || !RespawnInvulnerabilityEffect || RespawnInvulnerabilityDuration <= 0.0f)
	{
		return;
	}

	APlayerState* PlayerState = Controller->GetPlayerState<APlayerState>();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] %s의 리스폰 무적 ASC를 찾지 못했다."), *GetNameSafe(Controller));
		return;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(Controller->GetPawn());
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(RespawnInvulnerabilityEffect, 1.0f, EffectContext);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 리스폰 무적 Effect Spec을 만들지 못했다: %s"), *GetNameSafe(RespawnInvulnerabilityEffect));
		return;
	}

	Spec.Data->SetDuration(RespawnInvulnerabilityDuration, true);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
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
		// CDO는 월드 트랜스폼이 갱신되지 않아 Scaled 값에 BP 스케일이 빠질 수 있다.
		// bp에서 지정한 루트 스케일을 직접 곱해서 실제 캡슐 높이를 구함
		const UCapsuleComponent* Capsule = CharacterCDO->GetCapsuleComponent();
		const float HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight() * Capsule->GetRelativeScale3D().Z;
		return HalfHeight + Margin;
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
