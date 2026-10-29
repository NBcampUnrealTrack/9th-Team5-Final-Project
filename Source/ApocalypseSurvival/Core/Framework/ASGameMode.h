#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/Framework/ASGameTypes.h"
#include "ASGameMode.generated.h"

class AASGameState;
class AASRespawnPoint;
class UASPlayerRecordComponent;
class UASMapGameFlowComponent;
class UASVisionSetupComponent;
class UASZoneEffectComponent;
class UDataTable;
class UGameplayEffect;
class AASPlayerCharacter;
class AASLootChest;
class APlayerStart;
class AASMonsterCharacter;
class AASBossEncounter;

UCLASS()
class APOCALYPSESURVIVAL_API AASGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AASGameMode();
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	bool ReturnToLobby();
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FString LobbyMapPath = TEXT("/Game/ApocalypseSurvival/Maps/Lobby_Proto");
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal = TEXT("")) override;
	
	// 스폰할 캐릭터 BP: PlayerState에 정해진 캐릭터(리스폰) -> 캐릭터 선택 -> DefaultPawnClass 순
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	
	// 서버 전용. 클라이언트의 캐릭터 선택이 도착했을 때 PlayerState가 호출한다. 아직 스폰 전이면 스폰한다.
	void NotifyCharacterSelectionReceived(APlayerController* PlayerController);
	
	// 규칙 조회 (일차별 데이터 교체 가능)
	virtual float GetDayDuration(int32 Day) const;
	virtual float GetNightDuration(int32 Day) const;
	virtual bool IsBossDay(int32 Day) const;
	// 마지막 날 보스인가
	virtual bool IsFinalBossDay(int32 Day) const;
	virtual bool IsDeathCountEnabled() const;
	
	// 다른 시스템이 구독하는 이벤트 (서버 전용)
	FOnNightStarted OnNightStarted;
	FOnNightEnded OnNightEnded;
	
	// 스폰 시스템에서 호출 (서버 전용)
	// 이번 밤에 스폰한 적 수를 알려줌 (웨이브 결과 계산용)
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void ReportWaveSpawned(int32 Count);
	
	// 적이 죽었을 때 호출. Killer는 처치한 플레이어의 컨트롤러 (없으면 nullptr)
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void OnEnemyKilled(AController* Killer);

	/** Mass Processor처럼 한 프레임에 여러 적이 죽는 경로에서 집계를 한 번에 반영한다. */
	void OnEnemiesKilled(int32 Count, AController* Killer = nullptr);
	
	// 보스가 죽었을 때 호출
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void OnBossDefeated();
	bool IsActiveBoss(const AASMonsterCharacter* Monster) const { return ActiveBoss == Monster; }

	// 밤이 끝날 때 웨이브 결과 (보상 시스템 등이 참조)
	UPROPERTY(BlueprintAssignable, Category = "GameFlow")
	FOnWaveEnded OnWaveEnded;
	
	// 사망과 리스폰 (플레이어 쪽에서 호출)
	// 플레이어가 죽었을 때 호출. 데스카운트 초과면 패배, 아니면 리스폰
	UFUNCTION(BlueprintCallable, Category = "Death")
	void OnPlayerDied(AController* DeadController);

	// 멀티 규칙(쓰러짐, 부활, 리스폰 없음)을 쓰는가. 혼자 실행(Standalone)이면 기존 싱글 규칙
bool IsMultiplayerRules() const;
	
	// 체력이 0이 됐을 때 사망 대신 쓰러지는가 (멀티 규칙)
	bool IsDownedEnabled() const;
	
	// 플레이어가 쓰러졌을 때 캐릭터가 호출 (서버 전용). 전멸이면 패배
	void OnPlayerDowned(AController* DownedController);
	
	// 관전 중인 플레이어가 좌클릭으로 다음 파티원 화면으로 넘길 때 캐릭터가 호출 (서버 전용)
	void SpectateNext(APlayerController* Spectator);
	
	// 팀 부활 횟수를 다 썼을 때 캐릭터가 호출 (서버 전용).
	// 쓰러져 있는 사람은 더 살릴 수 없으므로 바로 사망 처리한다. (임시 규칙, 팀 상의 예정)
	void KillUnrevivablePlayers();
		
	// 맵에 배치된 리스폰 포인트 목록
	TArray<AASRespawnPoint*> GetAvailableRespawnPoints() const;
		
	
	// 디버그 (콘솔에서 입력)
	UFUNCTION(Exec)
	void DebugSkipPhase();
	
	UFUNCTION(Exec)
	void DebugAddKillCount();
	
	UFUNCTION(Exec)
	void DebugKillBoss();
	
	UFUNCTION(Exec)
	void DebugKillPlayer();
	
protected:
	virtual void HandleMatchHasStarted() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	
	// 원격 클라이언트는 캐릭터 선택이 도착할 때까지 스폰하지 않는다.
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

	// 클라이언트 캐릭터 선택을 기다리는 최대 시간. 넘으면 기본 캐릭터로 스폰
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Spawn", meta = (ClampMin = "0.5", Units = "s"))
	float CharacterSelectTimeout = 5.f;
	
	// 멀티: 팀 공용 부활 횟수 (임시값). 다 쓰면 체력 0일 때 쓰러지지 않고 바로 사망
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Revive", meta = (ClampMin = "0"))
	int32 TeamReviveCount = 3;
	
	// 멀티: 사망자 인벤토리를 담는 시체 루팅 상자 (BP_CorpseLoot). 비면 사라지고 시체도 숨긴다
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Death")
	TSubclassOf<AASLootChest> CorpseLootClass;
	
	// 게임 시작 전 대기 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Time")
	float ReadyDuration = 3.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Time")
	float DayDuration = 140.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Time")
	float NightDuration = 110.f;

	/**
	 * 모든 몬스터 스탯 Curve에 전달하는 스테이지 난이도 입력값이다.
	 * 현재는 스테이지 고정값이며, 추후 난이도 선택 화면이 생기면 GameState의 Setter만 호출하면 된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Monster Stats", meta = (ClampMin = "0.0"))
	float MonsterStatDifficulty = 1.0f;
	
	// 보스 등장일
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Boss")
	TArray<int32> BossDays = {3, 5};

	UPROPERTY(EditDefaultsOnly, Category = "Rule|Boss")
	TSoftClassPtr<AASMonsterCharacter> BossClass = TSoftClassPtr<AASMonsterCharacter>(
		FSoftObjectPath(TEXT("/Game/ApocalypseSurvival/Characters/Enemy/Blueprints/BP_BossMonster.BP_BossMonster_C")));

	UPROPERTY(Transient)
	TObjectPtr<AASMonsterCharacter> ActiveBoss;

	UPROPERTY(Transient)
	TObjectPtr<AASBossEncounter> BossEncounter;
	
	// 싱글 전용
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn")
	int32 MaxDeathCount = 1;
	
	// 0 = 무제한
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn")
	float RespawnSelectTimeLimit = 0.f;
	
	// 사망 후 리스폰까지 대기 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn")
	float RespawnDelay = 3.f;

	/** 리스폰 직후 자신에게 적용할 무적 효과. BP에서 GE_Invulnerable을 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn")
	TSubclassOf<UGameplayEffect> RespawnInvulnerabilityEffect;

	/** 0이면 리스폰 무적을 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnInvulnerabilityDuration = 3.f;
	
	// 행 이름 Wave_01, Wave_02 ... 에 따라 날짜별 스웜을 정의한 DataTable.
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Wave")
	TObjectPtr<UDataTable> WaveDataTable;

	// 행 이름 Day_01, Day_02 ... 에 따라 날짜별 필드 몬스터 배치를 정의한 DataTable.
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Day Spawn")
	TObjectPtr<UDataTable> DaySpawnDataTable;

	// 행 이름 MonsterGroup_00, MonsterGroup_01 ... 에 따라 낮 스폰과 밤 웨이브가 공통으로 쓰는 몬스터 구성을 정의한 DataTable.
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Monster Spawn")
	TObjectPtr<UDataTable> MonsterSpawnGroupDataTable;

	// DayNumber and SecondsAfterDayStart rows that schedule server-side loot chest spawns.
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Day Spawn|Loot",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/ApocalypseSurvival.ASLootDaySpawnScheduleRow"))
	TObjectPtr<UDataTable> LootDaySpawnScheduleDataTable;
	
	// 단계 진행
	void StartDay(int32 Day);
	void StartNight();
	void OnDayTimeUp();
	void OnNightTimeUp();
	void EndGame(bool bVictory);
	
	// 밤에 웨이브 몬스터 모두 처치 시 밤 종료 (보스 날은 보스 포함)
	void TryEndNightEarly();
	// 보스를 잡았는가
	bool bBossDefeatedThisNight = false;
	
	// 사망과 리스폰
	UASPlayerRecordComponent* GetOrCreateRecord(AController* Controller) const;
	void RespawnPlayer(AController* Controller);
	void ApplyRespawnInvulnerability(AController* Controller) const;
	
	// 리스폰 시 바닥에 박히지 않도록 높이 보간 (캡슐 절반 높이, 없으면 100cm)
	float GetSpawnHeightOffset(AController* Controller);
	APlayerStart* FindAvailablePlayerStart(AController* Controller, FTransform& OutSpawnTransform,
		bool& bOutUsedAdjustedLocation);
	int32 GetPlayerStartIndex(const AController* Controller, int32 StartCount);
	
	bool IsGameEnded() const;
	FWaveResult BuildWaveResult() const;
	
	AASGameState* GetASGameState() const;
	
	FTimerHandle PhaseTimerHandle;

private:
	// 서 있는 플레이어(살아 있고 쓰러지지 않음)가 한 명도 없으면 패배
	void CheckPartyWipe();
	
	// 관전 시작 (또는 대상 다시 고르기): 지금 보는 곳에서 가장 가까운 파티원 화면으로
	void StartSpectating(APlayerController* Spectator);
	
	// 관젅할 수 있는 파티원 (사망하지 않은 플레이어 캐릭터, 쓰러진 사람 포함). 관전자 자신은 제외
	TArray<AASPlayerCharacter*> GetSpectateCandidates(const APlayerController* Spectator) const;
	
	// 보고 있던 파티원이 사망하면, 그 파티원을 보던 관전자들을 다른 파티원으로 옮긴다
	void ReassignSpectatorsWatching(const AActor* LostTarget);
	
	// 관전 중인 플레이어 (멀티에서 사망한 플레이어)
	TArray<TWeakObjectPtr<APlayerController>> Spectators;

	// 퇴장 처리 중인 컨트롤러. Logout 중에는 아직 월드 목록에 남아 있어서 판정에서 직접 제외한다
	TWeakObjectPtr<AController> LeavingController;
	
	// 시체 위치에 루팅 상자를 만들고 사망자 인벤토리를 옮긴다. 옮길 게 없으면 시체를 바로 숨긴다
	void SpawnCorpseLoot(AASPlayerCharacter* Corpse, AController* DeadController);
	
	// 시체 루팅 상자가 비어서 사라지면 시체를 숨긴다 (상자의 Owner = 시체)
	UFUNCTION()
	void HandleCorpseLootDestroyed(AActor* DestroyedActor);
	
	// 맵 시스템(구역·시야·구역 효과). 구역 액터가 없는 레벨에서는 동작하지 않는다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASMapGameFlowComponent> MapFlow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASVisionSetupComponent> VisionSetup;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASZoneEffectComponent> ZoneEffects;
};
