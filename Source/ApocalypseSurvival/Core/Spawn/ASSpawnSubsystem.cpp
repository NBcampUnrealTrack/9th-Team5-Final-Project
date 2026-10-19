#include "Core/Spawn/ASSpawnSubsystem.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Core/Framework/ASGameMode.h"
#include "Core/Spawn/ASDaySpawnData.h"
#include "Core/Spawn/ASMonsterSpawnPoint.h"
#include "Core/Spawn/ASMonsterSpawnGroupData.h"
#include "Core/Spawn/ASWaveData.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MassEntityConfigAsset.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
	struct FASResolvedMonsterSpawn
	{
		EASMonsterSpawnBackend SpawnBackend = EASMonsterSpawnBackend::Actor;
		TSubclassOf<AASMonsterCharacter> MonsterClass;
		UMassEntityConfigAsset* MassEntityConfig = nullptr;
	};
}

bool UASSpawnSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

void UASSpawnSubsystem::Deinitialize()
{
	StopAllSpawning();
	ManagedMonsters.Reset();
	PlacedSpawnPoints.Reset();
	GeneratedWaveSpawnPoints.Reset();
	Super::Deinitialize();
}

void UASSpawnSubsystem::HandleDayStarted(int32 Day, UDataTable* DaySpawnDataTable, UDataTable* MonsterSpawnGroupDataTable)
{
	SpawnDayUnits(Day, DaySpawnDataTable, MonsterSpawnGroupDataTable);
}

void UASSpawnSubsystem::HandleNightStarted(int32 Day, UDataTable* WaveDataTable)
{
	int32 ExistingMonsterCount = CountAliveManagedActorMonsters();
	if (UASMassMonsterSubsystem* MassMonsterSubsystem = GetWorld()->GetSubsystem<UASMassMonsterSubsystem>())
	{
		ExistingMonsterCount += MassMonsterSubsystem->SetManagedMonsterWorldBehavior(
			EASMassMonsterWorldBehavior::NightAssault);
	}

	if (AASGameMode* GameMode = GetWorld()->GetAuthGameMode<AASGameMode>())
	{
		if (ExistingMonsterCount > 0)
		{
			GameMode->ReportWaveSpawned(ExistingMonsterCount);
			UE_LOG(LogTemp, Log, TEXT("[SpawnSubsystem] 이전 단계 생존 몬스터 %d마리를 이번 밤 웨이브에 포함"), ExistingMonsterCount);
		}
	}

	StartWave(Day, WaveDataTable);
}

void UASSpawnSubsystem::HandleNightEnded()
{
	StopWave();
	if (UASMassMonsterSubsystem* MassMonsterSubsystem = GetWorld()->GetSubsystem<UASMassMonsterSubsystem>())
	{
		MassMonsterSubsystem->SetManagedMonsterWorldBehavior(EASMassMonsterWorldBehavior::DayTerritory);
	}
}

void UASSpawnSubsystem::StopAllSpawning()
{
	StopWave();
}

void UASSpawnSubsystem::SpawnDayUnits(int32 Day, UDataTable* DaySpawnDataTable, UDataTable* MonsterSpawnGroupDataTable)
{
	if (!DaySpawnDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] DaySpawnDataTable이 설정되지 않아 %d일차 필드 몬스터를 생성하지 않음"), Day);
		return;
	}
	if (!MonsterSpawnGroupDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] MonsterSpawnGroupDataTable이 설정되지 않아 %d일차 필드 몬스터를 생성하지 않음"), Day);
		return;
	}

	const FName RowName(*FString::Printf(TEXT("Day_%02d"), Day));
	const FASDaySpawnDataRow* SpawnData = DaySpawnDataTable->FindRow<FASDaySpawnDataRow>(RowName, TEXT("SpawnDayUnits"));
	if (!SpawnData)
	{
		UE_LOG(LogTemp, Log, TEXT("[SpawnSubsystem] %s 규칙이 없어 필드 몬스터를 생성하지 않음"), *RowName.ToString());
		return;
	}

	CachePlacedSpawnPoints();
	const TArray<AASMonsterSpawnPoint*> SelectedPoints = SelectPlacedSpawnPoints(SpawnData->SpawnCount);
	if (SelectedPoints.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] %s에 사용할 AASMonsterSpawnPoint가 없음"), *RowName.ToString());
		return;
	}

	TArray<const FASDayMonsterGroupSelectionEntry*> ValidSelections;
	float TotalWeight = 0.0f;
	for (const FASDayMonsterGroupSelectionEntry& Selection : SpawnData->MonsterGroupSelections)
	{
		if (Selection.GroupWeight > 0.0f)
		{
			ValidSelections.Add(&Selection);
			TotalWeight += Selection.GroupWeight;
		}
	}
	if (TotalWeight <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] %s에 유효한 몬스터 그룹 가중치가 없음"), *RowName.ToString());
		return;
	}

	int32 SpawnedCount = 0;
	for (AASMonsterSpawnPoint* Point : SelectedPoints)
	{
		const float RandomWeight = FMath::FRandRange(0.0f, TotalWeight);
		float AccumulatedWeight = 0.0f;
		const FASDayMonsterGroupSelectionEntry* SelectedGroup = ValidSelections.Last();
		for (const FASDayMonsterGroupSelectionEntry* Candidate : ValidSelections)
		{
			AccumulatedWeight += Candidate->GroupWeight;
			if (RandomWeight <= AccumulatedWeight)
			{
				SelectedGroup = Candidate;
				break;
			}
		}

		if (SelectedGroup && IsValid(Point))
		{
			SpawnedCount += SpawnDayGroup(*SelectedGroup, MonsterSpawnGroupDataTable, *Point);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SpawnSubsystem] %s에서 %d개 포인트로 필드 몬스터 %d마리 생성"),
		*RowName.ToString(), SelectedPoints.Num(), SpawnedCount);
}

int32 UASSpawnSubsystem::SpawnDayGroup(const FASDayMonsterGroupSelectionEntry& GroupSelection,
	UDataTable* MonsterSpawnGroupDataTable, const AASMonsterSpawnPoint& SpawnPoint)
{
	const FASMonsterSpawnGroupDataRow* MonsterGroupData = FindMonsterGroupData(
		MonsterSpawnGroupDataTable, GroupSelection.MonsterGroupIndex);
	if (!MonsterGroupData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] MonsterSpawnGroupDataTable에 GroupIndex %d 행이 없음"),
			GroupSelection.MonsterGroupIndex);
		return 0;
	}

	TArray<FASResolvedMonsterSpawn> PendingMonsters;
	for (const FASMonsterSpawnGroupUnitEntry& Unit : MonsterGroupData->Units)
	{
		if (Unit.Count <= 0)
		{
			continue;
		}

		FASResolvedMonsterSpawn ResolvedSpawn;
		ResolvedSpawn.SpawnBackend = Unit.SpawnBackend;
		if (Unit.SpawnBackend == EASMonsterSpawnBackend::Mass)
		{
			ResolvedSpawn.MassEntityConfig = Unit.MassEntityConfig.LoadSynchronous();
			if (!ResolvedSpawn.MassEntityConfig)
			{
				UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] 그룹 %d의 Mass Entity Config가 비어 있어 제외함"),
					GroupSelection.MonsterGroupIndex);
				continue;
			}
		}
		else
		{
			ResolvedSpawn.MonsterClass = Unit.MonsterClass.LoadSynchronous();
			if (!ResolvedSpawn.MonsterClass)
			{
				UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] 그룹 %d의 몬스터 클래스 하나가 비어 있어 제외함"),
					GroupSelection.MonsterGroupIndex);
				continue;
			}
		}

		for (int32 UnitIndex = 0; UnitIndex < Unit.Count; ++UnitIndex)
		{
			PendingMonsters.Add(ResolvedSpawn);
		}
	}

	const TArray<FVector> SpawnLocations = FindDistributedSpawnLocations(SpawnPoint, PendingMonsters.Num());
	int32 SpawnedCount = 0;
	TMap<UMassEntityConfigAsset*, TArray<FTransform>> MassSpawnTransforms;
	for (int32 Index = 0; Index < PendingMonsters.Num() && Index < SpawnLocations.Num(); ++Index)
	{
		const FTransform SpawnTransform(SpawnPoint.GetActorRotation(), SpawnLocations[Index]);
		const FASResolvedMonsterSpawn& PendingMonster = PendingMonsters[Index];
		if (PendingMonster.SpawnBackend == EASMonsterSpawnBackend::Mass)
		{
			MassSpawnTransforms.FindOrAdd(PendingMonster.MassEntityConfig).Add(SpawnTransform);
		}
		else if (SpawnMonster(PendingMonster.MonsterClass, SpawnTransform, true, &SpawnPoint))
		{
			++SpawnedCount;
		}
	}

	for (TPair<UMassEntityConfigAsset*, TArray<FTransform>>& MassSpawnGroup : MassSpawnTransforms)
	{
		if (MassSpawnGroup.Key)
		{
			SpawnedCount += SpawnMassMonsters(*MassSpawnGroup.Key, MassSpawnGroup.Value,
				EASMassMonsterWorldBehavior::DayTerritory, &SpawnPoint);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SpawnSubsystem] 그룹 %d에서 %d마리 생성"), GroupSelection.MonsterGroupIndex, SpawnedCount);
	return SpawnedCount;
}

void UASSpawnSubsystem::StartWave(int32 Day, UDataTable* WaveDataTable)
{
	StopWave();

	if (!WaveDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[SpawnSubsystem] WaveDataTable이 설정되지 않아 웨이브를 스폰할 수 없음"));
		return;
	}

	const FName RowName(*FString::Printf(TEXT("Wave_%02d"), Day));
	const FASWaveDataRow* WaveData = WaveDataTable->FindRow<FASWaveDataRow>(RowName, TEXT("StartWave"));
	if (!WaveData)
	{
		UE_LOG(LogTemp, Error, TEXT("[SpawnSubsystem] WaveDataTable에 %s 행이 없음"), *RowName.ToString());
		return;
	}

	ActiveWaveSpawnBatchSize = FMath::Max(1, WaveData->SpawnBatchSize);
	ActiveWaveSpawnInterval = FMath::Max(0.01f, WaveData->SpawnInterval);
	for (const FASWaveUnitEntry& Unit : WaveData->Units)
	{
		if (Unit.Count <= 0)
		{
			continue;
		}

		FASPendingWaveMonsterSpawn PendingSpawn;
		PendingSpawn.SpawnBackend = Unit.SpawnBackend;
		if (Unit.SpawnBackend == EASMonsterSpawnBackend::Mass)
		{
			PendingSpawn.MassEntityConfig = Unit.MassEntityConfig.LoadSynchronous();
			if (!PendingSpawn.MassEntityConfig)
			{
				UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] %s의 Mass Entity Config가 비어 있어 제외함"), *RowName.ToString());
				continue;
			}
		}
		else
		{
			PendingSpawn.MonsterClass = Unit.MonsterClass.LoadSynchronous();
			if (!PendingSpawn.MonsterClass)
			{
				UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] %s의 몬스터 클래스가 비어 있어 제외함"), *RowName.ToString());
				continue;
			}
		}

		for (int32 Index = 0; Index < Unit.Count; ++Index)
		{
			PendingWaveMonsters.Add(PendingSpawn);
		}
	}

	for (int32 Index = PendingWaveMonsters.Num() - 1; Index > 0; --Index)
	{
		PendingWaveMonsters.Swap(Index, FMath::RandRange(0, Index));
	}

	TotalWaveMonsterCount = PendingWaveMonsters.Num();
	if (TotalWaveMonsterCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] %s에 유효한 스폰 유닛이 없음"), *RowName.ToString());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SpawnSubsystem] %s 스웜 시작: 총 %d마리, 틱당 %d마리 / %.2f초"),
		*RowName.ToString(), TotalWaveMonsterCount, ActiveWaveSpawnBatchSize, ActiveWaveSpawnInterval);

	SpawnWaveTick();
	if (SpawnedWaveMonsterCount < TotalWaveMonsterCount)
	{
		GetWorld()->GetTimerManager().SetTimer(WaveSpawnTimerHandle, this, &UASSpawnSubsystem::SpawnWaveTick,
			ActiveWaveSpawnInterval, true);
	}
}

void UASSpawnSubsystem::StopWave()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveSpawnTimerHandle);
	}

	PendingWaveMonsters.Reset();
	TotalWaveMonsterCount = 0;
	SpawnedWaveMonsterCount = 0;
}

void UASSpawnSubsystem::SpawnWaveTick()
{
	UWorld* World = GetWorld();
	AASGameMode* GameMode = World ? World->GetAuthGameMode<AASGameMode>() : nullptr;
	if (!GameMode || SpawnedWaveMonsterCount >= TotalWaveMonsterCount)
	{
		StopWave();
		return;
	}

	const int32 RequestedCount = FMath::Min(ActiveWaveSpawnBatchSize, PendingWaveMonsters.Num());
	TArray<FVector> Locations;
	if (RequestedCount <= 0 || !FindWaveSpawnLocations(RequestedCount, Locations))
	{
		return;
	}

	int32 SpawnedThisTick = 0;
	TArray<int32> SuccessfulIndices;
	TMap<UMassEntityConfigAsset*, TArray<int32>> MassIndicesByConfig;
	for (int32 AttemptIndex = 0; AttemptIndex < RequestedCount; ++AttemptIndex)
	{
		const int32 PendingIndex = PendingWaveMonsters.Num() - 1 - AttemptIndex;
		if (!PendingWaveMonsters.IsValidIndex(PendingIndex) || !Locations.IsValidIndex(AttemptIndex))
		{
			continue;
		}

		const FASPendingWaveMonsterSpawn& PendingMonster = PendingWaveMonsters[PendingIndex];
		if (PendingMonster.SpawnBackend == EASMonsterSpawnBackend::Mass)
		{
			MassIndicesByConfig.FindOrAdd(PendingMonster.MassEntityConfig.Get()).Add(PendingIndex);
		}
		else if (SpawnMonster(PendingMonster.MonsterClass,
			FTransform(FRotator::ZeroRotator, Locations[AttemptIndex]), false))
		{
			SuccessfulIndices.Add(PendingIndex);
			++SpawnedThisTick;
		}
	}

	for (const TPair<UMassEntityConfigAsset*, TArray<int32>>& MassGroup : MassIndicesByConfig)
	{
		if (!MassGroup.Key)
		{
			continue;
		}

		TArray<FTransform> MassTransforms;
		MassTransforms.Reserve(MassGroup.Value.Num());
		for (const int32 PendingIndex : MassGroup.Value)
		{
			const int32 AttemptIndex = PendingWaveMonsters.Num() - 1 - PendingIndex;
			if (Locations.IsValidIndex(AttemptIndex))
			{
				MassTransforms.Emplace(FRotator::ZeroRotator, Locations[AttemptIndex]);
			}
		}

		const int32 MassSpawned = SpawnMassMonsters(*MassGroup.Key, MassTransforms,
			EASMassMonsterWorldBehavior::NightAssault);
		if (MassSpawned == MassTransforms.Num())
		{
			SuccessfulIndices.Append(MassGroup.Value);
			SpawnedThisTick += MassSpawned;
		}
	}

	SuccessfulIndices.Sort(TGreater<int32>());
	for (const int32 SuccessfulIndex : SuccessfulIndices)
	{
		PendingWaveMonsters.RemoveAt(SuccessfulIndex, 1, EAllowShrinking::No);
	}

	if (SpawnedThisTick > 0)
	{
		SpawnedWaveMonsterCount += SpawnedThisTick;
		GameMode->ReportWaveSpawned(SpawnedThisTick);
	}

	if (SpawnedWaveMonsterCount >= TotalWaveMonsterCount)
	{
		World->GetTimerManager().ClearTimer(WaveSpawnTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("[SpawnSubsystem] 스웜 투입 완료: %d마리"), SpawnedWaveMonsterCount);
	}
}

void UASSpawnSubsystem::CachePlacedSpawnPoints()
{
	PlacedSpawnPoints.Reset();
	for (TActorIterator<AASMonsterSpawnPoint> It(GetWorld()); It; ++It)
	{
		PlacedSpawnPoints.Add(*It);
	}
}

TArray<AASMonsterSpawnPoint*> UASSpawnSubsystem::SelectPlacedSpawnPoints(int32 Count) const
{
	TArray<AASMonsterSpawnPoint*> Candidates;
	for (const TWeakObjectPtr<AASMonsterSpawnPoint>& Point : PlacedSpawnPoints)
	{
		if (AASMonsterSpawnPoint* ResolvedPoint = Point.Get();
			ResolvedPoint && ResolvedPoint->bEnabled)
		{
			Candidates.Add(ResolvedPoint);
		}
	}

	for (int32 Index = Candidates.Num() - 1; Index > 0; --Index)
	{
		Candidates.Swap(Index, FMath::RandRange(0, Index));
	}

	Candidates.SetNum(FMath::Min(FMath::Max(0, Count), Candidates.Num()));
	return Candidates;
}

TArray<FVector> UASSpawnSubsystem::FindDistributedSpawnLocations(const AASMonsterSpawnPoint& SpawnPoint, int32 Count) const
{
	TArray<FVector> Locations;
	if (Count <= 0)
	{
		return Locations;
	}

	const FVector Center = SpawnPoint.GetActorLocation();
	const float Radius = FMath::Max(0.0f, SpawnPoint.ScatterRadius);
	const float MinSpacing = FMath::Max(0.0f, SpawnPoint.MinSpawnSpacing);
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	constexpr int32 MaxAttemptsPerMonster = 24;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FVector BestCandidate = Center;
		float BestNearestDistance = -1.0f;

		for (int32 Attempt = 0; Attempt < MaxAttemptsPerMonster; ++Attempt)
		{
			FVector Candidate = Center + FVector(FMath::RandPointInCircle(Radius), 0.0f);
			if (NavSys)
			{
				FNavLocation NavLocation;
				if (!NavSys->ProjectPointToNavigation(Candidate, NavLocation, FVector(100.0f, 100.0f, 500.0f)))
				{
					continue;
				}
				Candidate = NavLocation.Location;
			}

			float NearestDistance = TNumericLimits<float>::Max();
			for (const FVector& ExistingLocation : Locations)
			{
				NearestDistance = FMath::Min(NearestDistance, FVector::Dist2D(Candidate, ExistingLocation));
			}

			if (Locations.IsEmpty() || NearestDistance >= MinSpacing)
			{
				BestCandidate = Candidate;
				break;
			}

			if (NearestDistance > BestNearestDistance)
			{
				BestNearestDistance = NearestDistance;
				BestCandidate = Candidate;
			}
		}

		// 반경이 너무 작거나 지형이 막힌 경우에도 같은 위치에 겹치지 않도록 가장 먼 후보를 쓴다.
		Locations.Add(BestCandidate);
	}

	return Locations;
}

const FASMonsterSpawnGroupDataRow* UASSpawnSubsystem::FindMonsterGroupData(
	UDataTable* MonsterSpawnGroupDataTable, int32 GroupIndex) const
{
	if (!MonsterSpawnGroupDataTable)
	{
		return nullptr;
	}

	TArray<FASMonsterSpawnGroupDataRow*> Rows;
	MonsterSpawnGroupDataTable->GetAllRows(TEXT("FindMonsterGroupData"), Rows);
	for (const FASMonsterSpawnGroupDataRow* Row : Rows)
	{
		if (Row && Row->GroupIndex == GroupIndex)
		{
			return Row;
		}
	}

	return nullptr;
}

bool UASSpawnSubsystem::FindWaveSpawnLocations(int32 EnemyCount, TArray<FVector>& OutLocations)
{
	OutLocations.Reset();
	if (EnemyCount <= 0)
	{
		return false;
	}

	if (GeneratedWaveSpawnPoints.IsEmpty())
	{
		GenerateWaveSpawnPoints();
	}
	if (GeneratedWaveSpawnPoints.IsEmpty())
	{
		return false;
	}

	TArray<FVector> Candidates;
	for (const FVector& Point : GeneratedWaveSpawnPoints)
	{
		if (!IsNearAnyLivingPlayer(Point))
		{
			Candidates.Add(Point);
		}
	}
	if (Candidates.IsEmpty())
	{
		Candidates = GeneratedWaveSpawnPoints;
	}

	for (int32 Index = Candidates.Num() - 1; Index > 0; --Index)
	{
		Candidates.Swap(Index, FMath::RandRange(0, Index));
	}

	const int32 PointCount = FMath::Clamp(
		FMath::DivideAndRoundUp(EnemyCount, FMath::Max(1, WaveEnemiesPerSpawnPoint)),
		1,
		Candidates.Num());
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	constexpr int32 MaxAttemptsPerMonster = 24;
	const float MinSpawnSpacing = FMath::Max(0.0f, WaveMonsterMinSpawnSpacing);

	for (int32 Index = 0; Index < EnemyCount; ++Index)
	{
		const FVector& Point = Candidates[Index % PointCount];
		FVector BestLocation = Point;
		float BestNearestDistance = -1.0f;

		for (int32 Attempt = 0; Attempt < MaxAttemptsPerMonster; ++Attempt)
		{
			const FVector2D Offset = FMath::RandPointInCircle(WaveSpawnScatterRadius);
			FVector Candidate = Point + FVector(Offset, 0.0f);
			if (NavSys)
			{
				FNavLocation NavLocation;
				if (!NavSys->ProjectPointToNavigation(Candidate, NavLocation, FVector(100.0f, 100.0f, 500.0f)))
				{
					continue;
				}
				Candidate = NavLocation.Location;
			}

			float NearestDistance = TNumericLimits<float>::Max();
			for (const FVector& ExistingLocation : OutLocations)
			{
				NearestDistance = FMath::Min(NearestDistance, FVector::Dist2D(Candidate, ExistingLocation));
			}

			if (OutLocations.IsEmpty() || NearestDistance >= MinSpawnSpacing)
			{
				BestLocation = Candidate;
				break;
			}

			if (NearestDistance > BestNearestDistance)
			{
				BestNearestDistance = NearestDistance;
				BestLocation = Candidate;
			}
		}

		OutLocations.Add(BestLocation);
	}

	return true;
}

void UASSpawnSubsystem::GenerateWaveSpawnPoints()
{
	GeneratedWaveSpawnPoints.Reset();

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] NavMesh가 없어 밤 웨이브 후보 위치를 만들지 못함"));
		return;
	}

	const int32 MaxAttempts = WaveSpawnPointCount * 20;
	for (int32 Attempt = 0; Attempt < MaxAttempts && GeneratedWaveSpawnPoints.Num() < WaveSpawnPointCount; ++Attempt)
	{
		FNavLocation RandomPoint;
		if (!NavSys->GetRandomPoint(RandomPoint))
		{
			continue;
		}

		bool bTooCloseToOtherPoint = false;
		for (const FVector& Existing : GeneratedWaveSpawnPoints)
		{
			if (FVector::Dist2D(Existing, RandomPoint.Location) < WaveSpawnPointMinSpacing)
			{
				bTooCloseToOtherPoint = true;
				break;
			}
		}

		if (!bTooCloseToOtherPoint)
		{
			GeneratedWaveSpawnPoints.Add(RandomPoint.Location);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SpawnSubsystem] 밤 웨이브 후보 위치 %d개 생성"), GeneratedWaveSpawnPoints.Num());
}

bool UASSpawnSubsystem::IsNearAnyLivingPlayer(const FVector& Location) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		const AASPlayerCharacter* PlayerCharacter = Cast<AASPlayerCharacter>(Pawn);
		if (!Pawn || (PlayerCharacter && PlayerCharacter->IsDead()))
		{
			continue;
		}

		if (FVector::Dist2D(Pawn->GetActorLocation(), Location) < WaveSpawnMinDistance)
		{
			return true;
		}
	}
	return false;
}

AASMonsterCharacter* UASSpawnSubsystem::SpawnMonster(TSubclassOf<AASMonsterCharacter> MonsterClass,
	const FTransform& SpawnTransform, bool bCanAggroDuringDay, const AASMonsterSpawnPoint* HomeSpawnPoint)
{
	UWorld* World = GetWorld();
	if (!World || !MonsterClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AASMonsterCharacter* Monster = World->SpawnActorDeferred<AASMonsterCharacter>(
		MonsterClass, SpawnTransform, nullptr, nullptr, SpawnParams.SpawnCollisionHandlingOverride);
	if (Monster)
	{
		Monster->SetDayBehavior(bCanAggroDuringDay
			? EASMonsterDayBehavior::ProximityAggro
			: EASMonsterDayBehavior::NightOnly);
		if (HomeSpawnPoint)
		{
			// 포인트 중심이 아니라, 흩어져 생성된 이 개체 자신의 최초 좌표로 정확히 복귀한다.
			Monster->ConfigureSpawnHome(SpawnTransform.GetLocation(), HomeSpawnPoint->LeashRadius,
				HomeSpawnPoint->ReturnAcceptanceRadius, HomeSpawnPoint->ReturnHealthPercentPerSecond,
				HomeSpawnPoint->DayDetectionRadius, HomeSpawnPoint->DayLoseTargetRadius,
				HomeSpawnPoint->ReturnMoveSpeedMultiplier);
		}
		Monster->FinishSpawning(SpawnTransform);
		ManagedMonsters.Add(Monster);
	}
	return Monster;
}

int32 UASSpawnSubsystem::SpawnMassMonsters(
	UMassEntityConfigAsset& EntityConfig,
	const TConstArrayView<FTransform> SpawnTransforms,
	const EASMassMonsterWorldBehavior WorldBehavior,
	const AASMonsterSpawnPoint* HomeSpawnPoint)
{
	UASMassMonsterSubsystem* MassMonsterSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UASMassMonsterSubsystem>()
		: nullptr;
	if (!MassMonsterSubsystem || SpawnTransforms.IsEmpty())
	{
		return 0;
	}

	TArray<FMassEntityHandle> SpawnedEntities;
	const FASMassMonsterSpawnResult Result = MassMonsterSubsystem->SpawnMonsters(
		EntityConfig, SpawnTransforms, WorldBehavior, SpawnedEntities);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] Mass 몬스터 생성 실패: %s"), *Result.ErrorMessage);
		return 0;
	}

	if (HomeSpawnPoint)
	{
		FASMassMonsterHomeSettings HomeSettings;
		HomeSettings.LeashRadius = HomeSpawnPoint->LeashRadius;
		HomeSettings.ReturnAcceptanceRadius = HomeSpawnPoint->ReturnAcceptanceRadius;
		HomeSettings.DetectionRadius = HomeSpawnPoint->DayDetectionRadius;
		HomeSettings.LoseTargetRadius = HomeSpawnPoint->DayLoseTargetRadius;
		HomeSettings.ReturnHealthPercentPerSecond = HomeSpawnPoint->ReturnHealthPercentPerSecond;
		HomeSettings.ReturnMoveSpeedMultiplier = HomeSpawnPoint->ReturnMoveSpeedMultiplier;
		MassMonsterSubsystem->ConfigureMonsterHomes(SpawnedEntities, HomeSettings);
	}

	return Result.SpawnedCount;
}

int32 UASSpawnSubsystem::CountAliveManagedActorMonsters()
{
	int32 Count = 0;
	for (auto It = ManagedMonsters.CreateIterator(); It; ++It)
	{
		AASMonsterCharacter* Monster = It->Get();
		if (!IsValid(Monster) || Monster->IsActorBeingDestroyed())
		{
			It.RemoveCurrent();
			continue;
		}

		if (!Monster->IsDead())
		{
			++Count;
		}
	}
	return Count;
}
