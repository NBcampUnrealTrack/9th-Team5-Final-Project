#include "MapSystem/Zone/Manager/ASMapSystemManager.h"

#include "ApocalypseSurvival.h"
#include "AbilitySystemComponent.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ASAbilityTags.h"
#include "HAL/IConsoleManager.h"
#include "MapSystem/Shared/ASMapAbilityUtils.h"
#include "MapSystem/Shared/ASMapShapeUtils.h"
#include "MapSystem/Zone/Core/ASMapDefinition.h"
#include "MapSystem/Zone/Core/ASMapStateComponent.h"
#include "MapSystem/Zone/Core/ASMapWorldSubsystem.h"
#include "MapSystem/Zone/ASMapZone.h"

namespace ASMapManagerPrivate
{
	static TAutoConsoleVariable<bool> CVarDebugDraw(
		TEXT("AS.Map.DebugDraw"),
		false,
		TEXT("Draw zone areas, adjacency links, danger on the server."),
		ECVF_Cheat);

	AASMapSystemManager* FindManagerOrWarn(UWorld* World)
	{
		TActorIterator<AASMapSystemManager> It(World);
		AASMapSystemManager* Manager = It ? *It : nullptr;
		if (!Manager)
		{
			UE_LOG(LogApocalypseSurvival, Warning, TEXT("AS.Map: no map manager in this world (commands run on the server/host)."));
		}
		return Manager;
	}

	static FAutoConsoleCommandWithWorldAndArgs DumpCommand(
		TEXT("AS.Map.Dump"),
		TEXT("Log zone dangers and seed."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (const AASMapSystemManager* Manager = FindManagerOrWarn(World))
			{
				Manager->DumpState();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs StepCommand(
		TEXT("AS.Map.Step"),
		TEXT("AS.Map.Step <Day> <Day|Night>: send a step directly (bypasses the GameMode clock)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AASMapSystemManager* Manager = FindManagerOrWarn(World);
			if (!Manager || Args.Num() != 2)
			{
				return;
			}

			FASMapPhaseContext Context;
			Context.Day = FCString::Atoi(*Args[0]);
			if (Context.Day < 1
				|| (!Args[1].Equals(TEXT("Day"), ESearchCase::IgnoreCase) && !Args[1].Equals(TEXT("Night"), ESearchCase::IgnoreCase)))
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("Usage: AS.Map.Step <Day >= 1> <Day|Night>"));
				return;
			}
			Context.DayPhase = Args[1].Equals(TEXT("Night"), ESearchCase::IgnoreCase) ? EASDayPhase::Night : EASDayPhase::Day;
			Manager->HandlePhaseChanged(Context);
		}));
}

AASMapSystemManager::AASMapSystemManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetReplicates(false);
	bNetLoadOnClient = false;
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool AASMapSystemManager::InitializeMap(UASMapDefinition* InMapDefinition, UASMapStateComponent* InMapState)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (bMapRunning)
	{
		StopMap();
	}

	bMapReady = false;
	ValidationErrors.Reset();

	MapDefinition = InMapDefinition;
	MapState = InMapState;

	if (!MapDefinition)
	{
		ValidationErrors.Add(TEXT("InitializeMap: MapDefinition is null."));
	}

	if (!MapState.IsValid())
	{
		ValidationErrors.Add(TEXT("InitializeMap: MapState is null (does the GameState have UASMapStateComponent?)."));
	}

	TArray<FASZoneConfig> ZoneConfigs;
	if (MapDefinition)
	{
		MapDefinition->ValidateDefinition(ValidationErrors);
		CollectLevelZones();
		BuildZoneConfigs(ZoneConfigs);
		ValidateLevel(ZoneConfigs, ValidationErrors);
		LogZoneOverlaps();
	}

	if (UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this))
	{
		if (!ZoneRegisteredHandle.IsValid())
		{
			ZoneRegisteredHandle = Subsystem->OnZoneRegistered.AddUObject(this, &AASMapSystemManager::HandleZoneRegistered);
			ZoneUnregisteredHandle = Subsystem->OnZoneUnregistered.AddUObject(this, &AASMapSystemManager::HandleZoneUnregistered);
		}
	}

	if (ValidationErrors.Num() > 0)
	{
		UE_LOG(LogApocalypseSurvival, Error, TEXT("Map initialization failed with %d error(s); OnMapReady was not sent."), ValidationErrors.Num());
		for (const FString& Error : ValidationErrors)
		{
			UE_LOG(LogApocalypseSurvival, Error, TEXT("  %s"), *Error);
		}

		// 이전 초기화 성공의 준비 플래그를 남기지 않는다.
		if (UASMapStateComponent* State = MapState.Get())
		{
			if (State->GetSnapshot().bMapReady)
			{
				FASMapStateSnapshot Snapshot = State->GetSnapshot();
				Snapshot.bMapReady = false;
				Snapshot.bMapRunning = false;
				State->ServerWriteSnapshot(Snapshot);
			}
		}
		return false;
	}

	RuleEngine.Configure(ZoneConfigs);

	UASMapStateComponent* State = MapState.Get();
	State->ServerSetMapDefinition(MapDefinition);

	FASMapStateSnapshot Snapshot = State->GetSnapshot();
	RuleEngine.ResetMatch(0, Snapshot);
	Snapshot.bMapReady = true;
	Snapshot.bMapRunning = false;
	State->ServerWriteSnapshot(Snapshot);

	bMapReady = true;
	UE_LOG(LogApocalypseSurvival, Log, TEXT("Map ready: %d zones from %s."), ZoneConfigs.Num(), *MapDefinition->GetName());
	OnMapReady.Broadcast(this);
	return true;
}

bool AASMapSystemManager::StartMap(int32 InMatchSeed)
{
	if (!HasAuthority() || !bMapReady || !MapState.IsValid())
	{
		UE_LOG(LogApocalypseSurvival, Error, TEXT("StartMap called before the map was ready."));
		return false;
	}

	MatchSeed = InMatchSeed;

	UASMapStateComponent* State = MapState.Get();
	FASMapStateSnapshot Snapshot = State->GetSnapshot();
	Snapshot.MatchId += 1;
	RuleEngine.ResetMatch(MatchSeed, Snapshot);
	Snapshot.bMapRunning = true;
	State->ServerWriteSnapshot(Snapshot);

	bMapRunning = true;

	UE_LOG(LogApocalypseSurvival, Log, TEXT("Map started. MatchSeed=%d MatchId=%d"), MatchSeed, Snapshot.MatchId);
	return true;
}

void AASMapSystemManager::HandlePhaseChanged(const FASMapPhaseContext& PhaseContext)
{
	if (!HasAuthority() || !bMapRunning || !MapState.IsValid())
	{
		UE_LOG(LogApocalypseSurvival, Warning, TEXT("HandlePhaseChanged ignored: map is not running."));
		return;
	}

	const FASZoneDangerScheduleRow* Rule = MapDefinition->FindDangerScheduleRow(PhaseContext.Day);
	if (!Rule)
	{
		UE_LOG(LogApocalypseSurvival, Error, TEXT("No danger schedule row for day %d; phase was not processed."), PhaseContext.Day);
		return;
	}

	const TMap<FName, FASZonePhaseCounts> PhaseCounts = BuildPhaseCounts();
	FASMapStateSnapshot Working = MapState->GetSnapshot();
	FASMapEngineEvents Events;
	if (RuleEngine.BeginStep(Working, PhaseContext, *Rule, PhaseCounts, Events))
	{
		CommitEvents(Working, Events);
	}
}

void AASMapSystemManager::StopMap()
{
	if (!bMapRunning)
	{
		return;
	}

	bMapRunning = false;

	if (UASMapStateComponent* State = MapState.Get())
	{
		FASMapStateSnapshot Working = State->GetSnapshot();
		FASMapEngineEvents Events;
		Working.bMapRunning = false;
		CommitEvents(Working, Events);
	}

	UE_LOG(LogApocalypseSurvival, Log, TEXT("Map stopped. Final zone state kept for results."));
}

TMap<FName, FASZonePhaseCounts> AASMapSystemManager::BuildPhaseCounts() const
{
	TMap<FName, FASZonePhaseCounts> Counts;
	UWorld* World = GetWorld();
	const UASMapStateComponent* State = MapState.Get();
	if (!HasAuthority() || !World || !State)
	{
		return Counts;
	}

	// 빈 구역도 0으로 포함하며 호출할 때마다 새로 센다.
	for (const FASZoneRuntimeState& Zone : State->GetSnapshot().Zones)
	{
		Counts.Add(Zone.ZoneId);
	}
	for (AASMonsterCharacter* Monster : TActorRange<AASMonsterCharacter>(World))
	{
		if (IsValid(Monster) && !Monster->IsDead())
		{
			if (FASZonePhaseCounts* ZoneCounts = Counts.Find(FindZoneAtLocation(Monster->GetActorLocation())))
			{
				++ZoneCounts->MonsterCount;
			}
		}
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const APawn* Pawn = IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
			if (!IsValid(Pawn))
			{
				continue;
			}
			const UAbilitySystemComponent* AbilitySystem = ASMapAbilityUtils::FindAbilitySystemComponent(Pawn);
			if (AbilitySystem && !AbilitySystem->HasMatchingGameplayTag(ASGameplayTags::State_Dead))
			{
				if (FASZonePhaseCounts* ZoneCounts = Counts.Find(FindZoneAtLocation(Pawn->GetActorLocation())))
				{
					ZoneCounts->bHasPlayers = true;
				}
			}
		}
	}
	return Counts;
}

bool AASMapSystemManager::GetZoneState(FName ZoneId, FASZoneRuntimeState& OutState) const
{
	const UASMapStateComponent* State = MapState.Get();
	return State ? State->GetZoneState(ZoneId, OutState) : false;
}

FName AASMapSystemManager::FindZoneAtLocation(const FVector& Location) const
{
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this);
	return Subsystem ? Subsystem->FindZoneIdAtLocation(Location) : NAME_None;
}

void AASMapSystemManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMap();

	if (UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this))
	{
		Subsystem->OnZoneRegistered.Remove(ZoneRegisteredHandle);
		Subsystem->OnZoneUnregistered.Remove(ZoneUnregisteredHandle);
	}
	ZoneRegisteredHandle.Reset();
	ZoneUnregisteredHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void AASMapSystemManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// 스텝 규칙에는 주기 처리가 없다. PIE 연결선과 위험도만 잠깐씩 다시 그린다.
	if (bMapRunning && ASMapManagerPrivate::CVarDebugDraw.GetValueOnGameThread())
	{
		DrawDebug();
	}
}

void AASMapSystemManager::CommitEvents(const FASMapStateSnapshot& WorkingState, const FASMapEngineEvents& Events)
{
	WriteSnapshot(WorkingState);
	for (const FASZoneRuntimeState& ZoneState : WorkingState.Zones)
	{
		if (Events.ChangedZones.Contains(ZoneState.ZoneId))
		{
			OnZoneStateChanged.Broadcast(ZoneState.ZoneId, ZoneState);
		}
	}
}

void AASMapSystemManager::WriteSnapshot(const FASMapStateSnapshot& WorkingState)
{
	UASMapStateComponent* State = MapState.Get();
	if (!State)
	{
		return;
	}

	// MapState 스냅샷만 저장된 사본이다. 변경이 없으면 기록을 생략한다.
	const FASMapStateSnapshot& Current = State->GetSnapshot();
	const bool bChanged = Current.Zones != WorkingState.Zones
		|| Current.CurrentPhase != WorkingState.CurrentPhase
		|| Current.MatchId != WorkingState.MatchId
		|| Current.bMapReady != WorkingState.bMapReady
		|| Current.bMapRunning != WorkingState.bMapRunning;

	if (bChanged)
	{
		State->ServerWriteSnapshot(WorkingState);
	}
}

void AASMapSystemManager::CollectLevelZones() const
{
	UWorld* World = GetWorld();
	UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this);
	if (!World || !Subsystem)
	{
		return;
	}

	// 구역은 BeginPlay에서 직접 등록한다. 아직 플레이를 시작하지 않은 액터도 여기서 수집한다.
	for (TActorIterator<AASMapZone> It(World); It; ++It)
	{
		Subsystem->RegisterZone(*It);
	}
}

void AASMapSystemManager::BuildZoneConfigs(TArray<FASZoneConfig>& OutConfigs) const
{
	OutConfigs.Reset();
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	const TArray<AASMapZone*> Zones = Subsystem->GetZones();
	TMap<FName, int32> ConfigIndices;
	for (const AASMapZone* Zone : Zones)
	{
		ConfigIndices.Add(Zone->GetZoneId(), OutConfigs.Num());
		FASZoneConfig& Config = OutConfigs.AddDefaulted_GetRef();
		Config.ZoneId = Zone->GetZoneId();
		Config.InitialDangerLevel = Zone->GetInitialDangerLevel();
	}

	for (const AASMapZone* Zone : Zones)
	{
		const int32 ZoneIndex = ConfigIndices.FindChecked(Zone->GetZoneId());
		for (const AASMapZone* AdjacentZone : Zone->GetAdjacentZones())
		{
			if (!IsValid(AdjacentZone) || AdjacentZone->GetZoneId() == Zone->GetZoneId())
			{
				continue;
			}

			if (const int32* AdjacentIndex = ConfigIndices.Find(AdjacentZone->GetZoneId()))
			{
				// 어느 한쪽에만 적혀 있어도 같은 양방향 연결을 생성한다.
				OutConfigs[ZoneIndex].AdjacentZoneIds.AddUnique(AdjacentZone->GetZoneId());
				OutConfigs[*AdjacentIndex].AdjacentZoneIds.AddUnique(Zone->GetZoneId());
			}
		}
	}

	// 액터와 배열의 편집 순서에 관계없이 규칙 입력 순서를 일정하게 유지한다.
	for (FASZoneConfig& Config : OutConfigs)
	{
		Config.AdjacentZoneIds.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	}
}

bool AASMapSystemManager::ValidateLevel(const TArray<FASZoneConfig>& ZoneConfigs, TArray<FString>& OutErrors) const
{
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		OutErrors.Add(TEXT("ValidateLevel: map subsystem missing."));
		return false;
	}
	const int32 InitialErrorCount = OutErrors.Num();
	const TArray<AASMapZone*> LevelZones = Subsystem->GetZones();
	if (LevelZones.IsEmpty())
	{
		OutErrors.Add(TEXT("ValidateLevel: no zone actors in the level."));
	}
	TMap<FName, int32> LevelZoneCounts;
	for (const AASMapZone* Zone : LevelZones)
	{
		if (Zone->GetZoneId().IsNone())
		{
			OutErrors.Add(FString::Printf(TEXT("Zone actor %s has no ZoneId."), *Zone->GetName()));
		}
		else
		{
			LevelZoneCounts.FindOrAdd(Zone->GetZoneId())++;
		}
		if (Zone->GetInitialDangerLevel() < ASMap::MinDangerLevel || Zone->GetInitialDangerLevel() > ASMap::MaxDangerLevel)
		{
			OutErrors.Add(FString::Printf(TEXT("Zone actor %s: InitialDangerLevel is outside 1..5."), *Zone->GetName()));
		}
		if (!Zone->HasAreaShapes())
		{
			OutErrors.Add(FString::Printf(TEXT("Zone actor %s (%s) has no area shapes."), *Zone->GetName(), *Zone->GetZoneId().ToString()));
		}
		Zone->ValidateArea(OutErrors);
		for (const AASMapZone* AdjacentZone : Zone->GetAdjacentZones())
		{
			if (!IsValid(AdjacentZone))
			{
				OutErrors.Add(FString::Printf(TEXT("Zone actor %s lists a null adjacent zone."), *Zone->GetName()));
			}
			else if (AdjacentZone == Zone)
			{
				OutErrors.Add(FString::Printf(TEXT("Zone actor %s lists itself as adjacent."), *Zone->GetName()));
			}
			else if (!LevelZones.Contains(AdjacentZone))
			{
				OutErrors.Add(FString::Printf(TEXT("Zone actor %s lists an unregistered adjacent zone %s."), *Zone->GetName(), *AdjacentZone->GetName()));
			}
		}
	}
	for (const TPair<FName, int32>& Pair : LevelZoneCounts)
	{
		if (Pair.Value > 1)
		{
			OutErrors.Add(FString::Printf(TEXT("ZoneId '%s' is used by %d zone actors."), *Pair.Key.ToString(), Pair.Value));
		}
	}

	// 반대쪽에서만 연결했어도 이웃이 있는 것으로 판단한다.
	for (const FASZoneConfig& Config : ZoneConfigs)
	{
		if (Config.AdjacentZoneIds.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("Zone '%s' has no adjacent zone in the level."), *Config.ZoneId.ToString()));
		}
	}
	return OutErrors.Num() == InitialErrorCount;
}

void AASMapSystemManager::LogZoneOverlaps() const
{
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	// GetZones는 ZoneId 사전순이므로 겹친 위치는 앞쪽 구역으로 결정된다.
	const TArray<AASMapZone*> Zones = Subsystem->GetZones();
	for (int32 IndexA = 0; IndexA < Zones.Num(); ++IndexA)
	{
		const AASMapZone* ZoneA = Zones[IndexA];
		if (ZoneA->GetZoneId().IsNone())
		{
			continue;
		}
		TArray<TArray<FVector2D>> OutlinesA;
		ZoneA->GetAreaOutlines(OutlinesA);
		for (int32 IndexB = IndexA + 1; IndexB < Zones.Num(); ++IndexB)
		{
			const AASMapZone* ZoneB = Zones[IndexB];
			if (ZoneB->GetZoneId().IsNone())
			{
				continue;
			}
			TArray<TArray<FVector2D>> OutlinesB;
			ZoneB->GetAreaOutlines(OutlinesB);
			const bool bOverlaps = OutlinesA.ContainsByPredicate([&OutlinesB](const TArray<FVector2D>& OutlineA)
			{
				return OutlinesB.ContainsByPredicate([&OutlineA](const TArray<FVector2D>& OutlineB)
				{
					return ASMapShapeUtils::OutlinesOverlap(OutlineA, OutlineB);
				});
			});
			if (bOverlaps)
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("Zones '%s' and '%s' overlap; locations in the overlap resolve to '%s' (ZoneId order)."),
					*ZoneA->GetZoneId().ToString(), *ZoneB->GetZoneId().ToString(), *ZoneA->GetZoneId().ToString());
			}
		}
	}
}

void AASMapSystemManager::HandleZoneRegistered(AASMapZone* Zone)
{
	if (bMapReady && Zone && !RuleEngine.IsZoneKnown(Zone->GetZoneId()))
	{
		UE_LOG(LogApocalypseSurvival, Warning, TEXT("Zone actor %s registered after init with unknown id '%s'; it is ignored by the rules."),
			*Zone->GetName(), *Zone->GetZoneId().ToString());
	}
}

void AASMapSystemManager::HandleZoneUnregistered(AASMapZone* Zone)
{
	if (bMapRunning && Zone)
	{
		UE_LOG(LogApocalypseSurvival, Warning, TEXT("Zone actor %s (%s) unregistered while the map is running; location lookups will miss it."),
			*Zone->GetName(), *Zone->GetZoneId().ToString());
	}
}

void AASMapSystemManager::DumpState() const
{
	const UASMapStateComponent* State = MapState.Get();
	if (!State)
	{
		UE_LOG(LogApocalypseSurvival, Log, TEXT("Map: no state."));
		return;
	}

	const FASMapStateSnapshot& Snapshot = State->GetSnapshot();
	UE_LOG(LogApocalypseSurvival, Log, TEXT("=== Map state: ready=%d running=%d seed=%d match=%d phase=%s ==="),
		Snapshot.bMapReady ? 1 : 0, Snapshot.bMapRunning ? 1 : 0, MatchSeed, Snapshot.MatchId,
		*Snapshot.CurrentPhase.GetPhaseKey().ToString());

	const TMap<FName, FASZonePhaseCounts> Counts = BuildPhaseCounts();
	for (const FASZoneRuntimeState& Zone : Snapshot.Zones)
	{
		const FASZonePhaseCounts ZoneCounts = Counts.FindRef(Zone.ZoneId);
		UE_LOG(LogApocalypseSurvival, Log, TEXT("  %-12s danger=%d monsterCount=%d hasPlayers=%d"),
			*Zone.ZoneId.ToString(), Zone.DangerLevel, ZoneCounts.MonsterCount, ZoneCounts.bHasPlayers ? 1 : 0);
	}
}

void AASMapSystemManager::DrawDebug() const
{
	const UASMapStateComponent* State = MapState.Get();
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this);
	if (!State || !Subsystem || !MapDefinition)
	{
		return;
	}

	const float Duration = 0.15f;

	for (const AASMapZone* Zone : Subsystem->GetZones())
	{
		const FASZoneRuntimeState* ZoneState = State->GetSnapshot().FindZone(Zone->GetZoneId());

		FLinearColor Color = MapDefinition->GetDangerColor(ZoneState ? ZoneState->DangerLevel : 1);
		TArray<TArray<FVector2D>> Outlines;
		Zone->GetAreaOutlines(Outlines);
		const float Z = Zone->GetActorLocation().Z;
		for (const TArray<FVector2D>& Outline : Outlines)
		{
			for (int32 Index = 0; Index < Outline.Num(); ++Index)
			{
				const FVector A(Outline[Index], Z);
				const FVector B(Outline[(Index + 1) % Outline.Num()], Z);
				DrawDebugLine(GetWorld(), A, B, Color.ToFColor(true), false, Duration, 0, 10.0f);
			}
		}

		const FString Label = FString::Printf(TEXT("%s  D%d"),
			*Zone->GetZoneId().ToString(), ZoneState ? ZoneState->DangerLevel : Zone->GetInitialDangerLevel());

		DrawDebugString(GetWorld(), Zone->GetZoneCenter() + FVector(0.0, 0.0, 300.0), Label, nullptr, Color.ToFColor(true), Duration, true, 1.4f);

		// 초기화 때 규칙 엔진에 전달한 양방향 인접 관계를 그대로 표시한다.
		if (const FASZoneConfig* Config = RuleEngine.FindZoneConfig(Zone->GetZoneId()))
		{
			for (const FName& AdjacentId : Config->AdjacentZoneIds)
			{
				if (Config->ZoneId.LexicalLess(AdjacentId))
				{
					if (const AASMapZone* AdjacentZone = Subsystem->FindZone(AdjacentId))
					{
						DrawDebugLine(GetWorld(), Zone->GetZoneCenter(), AdjacentZone->GetZoneCenter(), FColor::Cyan, false, Duration, 0, 2.0f);
					}
				}
			}
		}
	}
}
