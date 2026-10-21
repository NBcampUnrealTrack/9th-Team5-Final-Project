#include "Mass/Monster/Testing/ASMassMonsterFunctionalTestActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ASAbilityTags.h"
#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityQuery.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterFunctionalTestActor)

namespace ASMassMonsterVisibilityStress
{
	constexpr uint8 BrightStateBit = 1 << 0;
	constexpr uint8 BufferStateBit = 1 << 1;
	constexpr uint8 CulledStateBit = 1 << 2;
	constexpr uint8 AllStateBits = BrightStateBit | BufferStateBit | CulledStateBit;
	constexpr int32 OnScreenMessageKey = 0x4D565331;
}

AASMassMonsterFunctionalTestActor::AASMassMonsterFunctionalTestActor()
{
	// Visibility Stress가 켜졌을 때만 매 프레임 표시 상태를 샘플링한다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// Mass Processing Phase가 모두 끝난 뒤의 표시 상태를 읽는다.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AASMassMonsterFunctionalTestActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SampleVisibilityStress();
}

void AASMassMonsterFunctionalTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(StartTimer, this,
		&AASMassMonsterFunctionalTestActor::TryStartTest, FMath::Max(0.01f, InitialDelay), false);
}

void AASMassMonsterFunctionalTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StartTimer);
	GetWorldTimerManager().ClearTimer(SpawnBatchTimer);
	GetWorldTimerManager().ClearTimer(StateReportTimer);
	GetWorldTimerManager().ClearTimer(BenchmarkSettleTimer);
	GetWorldTimerManager().ClearTimer(CsvCaptureTimer);
	GetWorldTimerManager().ClearTimer(BetweenCaptureTimer);
	GetWorldTimerManager().ClearTimer(TraceCaptureTimer);
	GetWorldTimerManager().ClearTimer(VisibilityReportTimer);
	GetWorldTimerManager().ClearTimer(VisibilityStressEndTimer);
	GetWorldTimerManager().ClearTimer(StressDayNightTimer);
	if (bCsvCaptureActive)
	{
		ExecuteBenchmarkCommand(TEXT("csvprofile stop"));
		bCsvCaptureActive = false;
	}
	if (bTraceCaptureActive)
	{
		ExecuteBenchmarkCommand(TEXT("Trace.Stop"));
		bTraceCaptureActive = false;
	}
	RemoveGrantedInvulnerability();
	Super::EndPlay(EndPlayReason);
}

void AASMassMonsterFunctionalTestActor::TryStartTest()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (MonsterCount > 0 && !IsValid(EntityConfig))
	{
		UE_LOG(LogTemp, Error, TEXT("[MassFunctionalTest] EntityConfig is not assigned."));
		return;
	}

	if (UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
	{
		GetWorldTimerManager().SetTimer(StartTimer, this,
			&AASMassMonsterFunctionalTestActor::TryStartTest, 0.25f, false);
		return;
	}

	if (bGrantInvulnerability)
	{
		GrantPlayerInvulnerability();
	}

	NextSpawnIndex = 0;
	if (MonsterCount == 0)
	{
		PendingSpawnTransforms.Reset();
		HandleSpawnComplete();
		return;
	}

	if (!BuildSpawnTransforms())
	{
		UE_LOG(LogTemp, Error, TEXT("[MassFunctionalTest] Could not create %d projected NavMesh spawn transforms."), MonsterCount);
		return;
	}

	SpawnNextBatch();
	if (NextSpawnIndex < PendingSpawnTransforms.Num())
	{
		GetWorldTimerManager().SetTimer(SpawnBatchTimer, this,
			&AASMassMonsterFunctionalTestActor::SpawnNextBatch, FMath::Max(0.01f, SpawnInterval), true);
	}
}

void AASMassMonsterFunctionalTestActor::SpawnNextBatch()
{
	UWorld* World = GetWorld();
	UASMassMonsterSubsystem* MonsterSubsystem = World ? World->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	if (!MonsterSubsystem || NextSpawnIndex >= PendingSpawnTransforms.Num())
	{
		GetWorldTimerManager().ClearTimer(SpawnBatchTimer);
		return;
	}

	const int32 BatchCount = FMath::Min(FMath::Max(1, SpawnBatchSize), PendingSpawnTransforms.Num() - NextSpawnIndex);
	TArray<FTransform> Batch;
	Batch.Append(PendingSpawnTransforms.GetData() + NextSpawnIndex, BatchCount);

	const FASMassMonsterSpawnResult Result = MonsterSubsystem->SpawnMonstersAtTransforms(EntityConfig, Batch, WorldBehavior);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogTemp, Error, TEXT("[MassFunctionalTest] Spawn failed at %d/%d: %s"),
			NextSpawnIndex, PendingSpawnTransforms.Num(), *Result.ErrorMessage);
		GetWorldTimerManager().ClearTimer(SpawnBatchTimer);
		return;
	}

	NextSpawnIndex += Result.SpawnedCount;
	if (NextSpawnIndex >= PendingSpawnTransforms.Num())
	{
		GetWorldTimerManager().ClearTimer(SpawnBatchTimer);
		HandleSpawnComplete();
	}
}

void AASMassMonsterFunctionalTestActor::HandleSpawnComplete()
{
	UE_LOG(LogTemp, Display, TEXT("[MassFunctionalTest] Spawn complete: %d Mass monsters."), NextSpawnIndex);
	GetWorldTimerManager().SetTimer(StateReportTimer, this,
		&AASMassMonsterFunctionalTestActor::ReportStateCounts, FMath::Max(0.01f, StateReportDelay), false);

	if (bEnableVisibilityStress)
	{
		StartVisibilityStress();
	}

	if (bEnableBenchmarkCapture)
	{
		UE_LOG(LogTemp, Display, TEXT("[MassBenchmark] Settle start N=%d Duration=%.2fs"),
			MonsterCount, BenchmarkSettleDelay);
		GetWorldTimerManager().SetTimer(BenchmarkSettleTimer, this,
			&AASMassMonsterFunctionalTestActor::StartCsvCapture, FMath::Max(0.01f, BenchmarkSettleDelay), false);
	}
}

void AASMassMonsterFunctionalTestActor::ReportStateCounts()
{
	UWorld* World = GetWorld();
	const UASMassMonsterSubsystem* MonsterSubsystem = World ? World->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	if (!MonsterSubsystem)
	{
		return;
	}

	const FASMassMonsterStateCounts Counts = MonsterSubsystem->GetManagedMonsterStateCounts();
	UE_LOG(LogTemp, Display,
		TEXT("[MassFunctionalTest] States Total=%d Idle=%d Chase=%d Attack=%d ReturnHome=%d Dead=%d"),
		Counts.Total, Counts.Idle, Counts.Chase, Counts.Attack, Counts.ReturnHome, Counts.Dead);
}

void AASMassMonsterFunctionalTestActor::StartCsvCapture()
{
	UE_LOG(LogTemp, Display, TEXT("[MassBenchmark] CSV start N=%d Duration=%.2fs"),
		MonsterCount, CsvCaptureDuration);
	ExecuteBenchmarkCommand(TEXT("csvprofile start"));
	bCsvCaptureActive = true;
	GetWorldTimerManager().SetTimer(CsvCaptureTimer, this,
		&AASMassMonsterFunctionalTestActor::StopCsvCapture, FMath::Max(1.0f, CsvCaptureDuration), false);
}

void AASMassMonsterFunctionalTestActor::StopCsvCapture()
{
	ExecuteBenchmarkCommand(TEXT("csvprofile stop"));
	bCsvCaptureActive = false;
	UE_LOG(LogTemp, Display, TEXT("[MassBenchmark] CSV stop N=%d"), MonsterCount);

	if (bCaptureTrace)
	{
		GetWorldTimerManager().SetTimer(BetweenCaptureTimer, this,
			&AASMassMonsterFunctionalTestActor::StartTraceCapture, FMath::Max(0.01f, BetweenCaptureDelay), false);
	}
	else
	{
		FinishBenchmark();
	}
}

void AASMassMonsterFunctionalTestActor::StartTraceCapture()
{
	UE_LOG(LogTemp, Display, TEXT("[MassBenchmark] Trace start N=%d Duration=%.2fs"),
		MonsterCount, TraceCaptureDuration);
	ExecuteBenchmarkCommand(TEXT("Trace.File cpu,frame,bookmark"));
	bTraceCaptureActive = true;
	GetWorldTimerManager().SetTimer(TraceCaptureTimer, this,
		&AASMassMonsterFunctionalTestActor::StopTraceCapture, FMath::Max(1.0f, TraceCaptureDuration), false);
}

void AASMassMonsterFunctionalTestActor::StopTraceCapture()
{
	ExecuteBenchmarkCommand(TEXT("Trace.Stop"));
	bTraceCaptureActive = false;
	UE_LOG(LogTemp, Display, TEXT("[MassBenchmark] Trace stop N=%d"), MonsterCount);
	FinishBenchmark();
}

void AASMassMonsterFunctionalTestActor::FinishBenchmark()
{
	ReportStateCounts();
	UE_LOG(LogTemp, Display, TEXT("[MassBenchmark] Done N=%d"), MonsterCount);
}

void AASMassMonsterFunctionalTestActor::StartVisibilityStress()
{
	if (bHoldMonstersInPlace)
	{
		HoldMonstersInPlace();
	}

	VisibilityStressTracks.Reset();
	VisibilityFlickerEvents = 0;
	VisibilityStressStartTime = GetWorld()->GetTimeSeconds();
	SetActorTickEnabled(true);

	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.SetTimer(VisibilityReportTimer, this,
		&AASMassMonsterFunctionalTestActor::ReportVisibilityStress, FMath::Max(0.05f, VisibilityReportInterval), true);
	TimerManager.SetTimer(VisibilityStressEndTimer, this,
		&AASMassMonsterFunctionalTestActor::FinishVisibilityStress, FMath::Max(1.0f, VisibilityStressDuration), false);
	if (StressDayNightHalfCycleSeconds > 0.0f)
	{
		// 낮에서 시작해 반주기마다 밤/낮을 바꾼다. 실제 GameMode 시간표는 건드리지 않는다.
		bStressNight = true;
		StressDayNightToggleCount = 0;
		ToggleStressDayNight();
		TimerManager.SetTimer(StressDayNightTimer, this,
			&AASMassMonsterFunctionalTestActor::ToggleStressDayNight, StressDayNightHalfCycleSeconds, true);
	}
	UE_LOG(LogTemp, Display, TEXT("[MassVisibilityStress] Start Monsters=%d Duration=%.1fs"),
		NextSpawnIndex, VisibilityStressDuration);
}

void AASMassMonsterFunctionalTestActor::HoldMonstersInPlace()
{
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassEntityQuery Query(EntityManager.AsShared());
	Query.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FASMassMonsterHomeFragment>(EMassFragmentAccess::ReadWrite);
	FMassExecutionContext Context(EntityManager, 0.0f, false);
	Query.ForEachEntityChunk(Context, [](FMassExecutionContext& ChunkContext)
	{
		// 테스트 전용: 플레이어를 감지하지 않아 시야 경계에 놓인 위치를 유지한다.
		for (FASMassMonsterHomeFragment& Home : ChunkContext.GetMutableFragmentView<FASMassMonsterHomeFragment>())
		{
			Home.DetectionRadius = 0.0f;
		}
	});
}

void AASMassMonsterFunctionalTestActor::SampleVisibilityStress()
{
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	using namespace ASMassMonsterVisibilityStress;
	const double Now = GetWorld()->GetTimeSeconds();
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassEntityQuery Query(EntityManager.AsShared());
	Query.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FASMassMonsterFogVisibilityFragment>(EMassFragmentAccess::ReadOnly);
	FMassExecutionContext Context(EntityManager, 0.0f, false);
	Query.ForEachEntityChunk(Context, [this, Now](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FASMassMonsterFogVisibilityFragment> FogVisibility =
			ChunkContext.GetFragmentView<FASMassMonsterFogVisibilityFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FASMassMonsterFogVisibilityFragment& FogState = FogVisibility[EntityIt];
			if (!FogState.bInitialized)
			{
				continue;
			}

			FVisibilityStressTrack& Track = VisibilityStressTracks.FindOrAdd(ChunkContext.GetEntity(EntityIt));
			Track.VisitedStateMask |= FogState.bVisibleToLocalTeam ? BrightStateBit
				: (FogState.bRetainedInFogBuffer ? BufferStateBit : CulledStateBit);
			if (Track.bHasState && Track.bVisible != FogState.bVisibleToLocalTeam)
			{
				// 직전 전환 직후 다시 되돌아가면 사용자가 보게 되는 깜빡임이다.
				if (Track.LastToggleTime >= 0.0 && Now - Track.LastToggleTime <= FlickerWindowSeconds)
				{
					++VisibilityFlickerEvents;
				}
				Track.LastToggleTime = Now;
				++Track.ToggleCount;
			}
			Track.bHasState = true;
			Track.bVisible = FogState.bVisibleToLocalTeam;
		}
	});
}

void AASMassMonsterFunctionalTestActor::ReportVisibilityStress()
{
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	int32 BrightCount = 0;
	int32 BufferHiddenCount = 0;
	int32 CulledCount = 0;
	int32 ISMCount = 0;
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassEntityQuery Query(EntityManager.AsShared());
	Query.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FASMassMonsterFogVisibilityFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	FMassExecutionContext Context(EntityManager, 0.0f, false);
	Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FASMassMonsterFogVisibilityFragment> FogVisibility =
			ChunkContext.GetFragmentView<FASMassMonsterFogVisibilityFragment>();
		const TConstArrayView<FMassRepresentationFragment> Representations =
			ChunkContext.GetFragmentView<FMassRepresentationFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FASMassMonsterFogVisibilityFragment& FogState = FogVisibility[EntityIt];
			if (FogState.bVisibleToLocalTeam)
			{
				++BrightCount;
			}
			else if (FogState.bRetainedInFogBuffer)
			{
				++BufferHiddenCount;
			}
			else
			{
				++CulledCount;
			}
			ISMCount += Representations[EntityIt].CurrentRepresentation == EMassRepresentationType::StaticMeshInstance ? 1 : 0;
		}
	});

	const UASVisionWorldSubsystem* VisionSubsystem = UASVisionWorldSubsystem::Get(GetWorld());
	const AASFogOfWarManager* FogManager = VisionSubsystem ? VisionSubsystem->GetFogOfWarManager() : nullptr;
	const float NightAlpha = FogManager ? FogManager->GetNightAlpha() : 0.0f;
	const FString Message = FString::Printf(
		TEXT("[MassVisibilityStress] t=%.2f Night=%.2f Bright=%d BufferHidden=%d Culled=%d ISM=%d Flicker=%d"),
		GetWorld()->GetTimeSeconds() - VisibilityStressStartTime, NightAlpha,
		BrightCount, BufferHiddenCount, CulledCount, ISMCount, VisibilityFlickerEvents);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(ASMassMonsterVisibilityStress::OnScreenMessageKey,
			VisibilityReportInterval + 0.1f, VisibilityFlickerEvents > 0 ? FColor::Red : FColor::Green, Message);
	}
}

void AASMassMonsterFunctionalTestActor::ToggleStressDayNight()
{
	const UASVisionWorldSubsystem* VisionSubsystem = UASVisionWorldSubsystem::Get(GetWorld());
	AASFogOfWarManager* FogManager = VisionSubsystem ? VisionSubsystem->GetFogOfWarManager() : nullptr;
	if (!FogManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[MassVisibilityStress] FogOfWarManager is missing; day/night cycle disabled."));
		GetWorldTimerManager().ClearTimer(StressDayNightTimer);
		return;
	}

	bStressNight = !bStressNight;
	++StressDayNightToggleCount;
	FogManager->SetNight(bStressNight);
	UE_LOG(LogTemp, Display, TEXT("[MassVisibilityStress] t=%.2f %s"),
		GetWorld()->GetTimeSeconds() - VisibilityStressStartTime, bStressNight ? TEXT("Night") : TEXT("Day"));
}

void AASMassMonsterFunctionalTestActor::FinishVisibilityStress()
{
	using namespace ASMassMonsterVisibilityStress;
	SampleVisibilityStress();
	ReportVisibilityStress();
	SetActorTickEnabled(false);
	GetWorldTimerManager().ClearTimer(VisibilityReportTimer);
	GetWorldTimerManager().ClearTimer(StressDayNightTimer);

	int32 AllStatesVisited = 0;
	int32 MaxToggles = 0;
	for (const TPair<FMassEntityHandle, FVisibilityStressTrack>& Pair : VisibilityStressTracks)
	{
		AllStatesVisited += Pair.Value.VisitedStateMask == AllStateBits ? 1 : 0;
		MaxToggles = FMath::Max(MaxToggles, Pair.Value.ToggleCount);
	}

	// 경계가 실제로 몬스터 위를 지나가지 않았다면 깜빡임 0은 의미가 없으므로 실패로 본다.
	const bool bPassed = VisibilityFlickerEvents == 0 && AllStatesVisited > 0;
	UE_LOG(LogTemp, Display,
		TEXT("[MassVisibilityStress] Done DayNightToggles=%d Tracked=%d AllStatesVisited=%d MaxToggles=%d Flicker=%d Result=%s"),
		StressDayNightToggleCount, VisibilityStressTracks.Num(), AllStatesVisited, MaxToggles, VisibilityFlickerEvents,
		bPassed ? TEXT("PASS") : TEXT("FAIL"));
}

void AASMassMonsterFunctionalTestActor::ExecuteBenchmarkCommand(const FString& Command)
{
	UKismetSystemLibrary::ExecuteConsoleCommand(this, Command, nullptr);
}

bool AASMassMonsterFunctionalTestActor::BuildSpawnTransforms()
{
	PendingSpawnTransforms.Reset();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!NavigationSystem || MonsterCount <= 0)
	{
		return false;
	}

	FVector Center = GetActorLocation();
	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		if (const APawn* PlayerPawn = PlayerController->GetPawn())
		{
			Center = PlayerPawn->GetActorLocation();
		}
	}

	const float SafeMinRadius = FMath::Max(0.0f, MinSpawnRadius);
	const float SafeMaxRadius = FMath::Max(SafeMinRadius + 1.0f, MaxSpawnRadius);
	constexpr double GoldenAngleRadians = 2.39996322972865332;
	const FVector ProjectionExtent(250.0f, 250.0f, 500.0f);

	for (int32 Index = 0; Index < MonsterCount; ++Index)
	{
		const double Fraction = (static_cast<double>(Index) + 0.5) / static_cast<double>(MonsterCount);
		const double RadiusSquared = FMath::Lerp(
			FMath::Square(static_cast<double>(SafeMinRadius)),
			FMath::Square(static_cast<double>(SafeMaxRadius)),
			Fraction);
		const double Radius = FMath::Sqrt(RadiusSquared);
		const double Angle = static_cast<double>(Index) * GoldenAngleRadians;
		const FVector DesiredLocation = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);

		FNavLocation ProjectedLocation;
		if (!NavigationSystem->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, ProjectionExtent))
		{
			continue;
		}

		const FRotator FacingRotation = (Center - ProjectedLocation.Location).Rotation();
		PendingSpawnTransforms.Emplace(FacingRotation, ProjectedLocation.Location);
	}

	return PendingSpawnTransforms.Num() == MonsterCount;
}

void AASMassMonsterFunctionalTestActor::GrantPlayerInvulnerability()
{
	ModifiedAbilitySystems.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		UAbilitySystemComponent* ASC = IsValid(PlayerPawn)
			? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerPawn)
			: nullptr;
		if (!IsValid(ASC))
		{
			continue;
		}

		ASC->AddLooseGameplayTag(ASGameplayTags::State_Invulnerable, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
		ModifiedAbilitySystems.Add(ASC);
	}
}

void AASMassMonsterFunctionalTestActor::RemoveGrantedInvulnerability()
{
	for (const TWeakObjectPtr<UAbilitySystemComponent>& WeakASC : ModifiedAbilitySystems)
	{
		if (UAbilitySystemComponent* ASC = WeakASC.Get())
		{
			ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Invulnerable, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
	ModifiedAbilitySystems.Reset();
}
