#include "Core/Spawn/ASSpawnSubsystem.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Core/Framework/ASGameMode.h"
#include "Core/Spawn/ASWaveData.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"

void UASSpawnSubsystem::Deinitialize()
{
	StopWave();
	Super::Deinitialize();
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

		const TSubclassOf<AASMonsterCharacter> MonsterClass = Unit.MonsterClass.LoadSynchronous();
		if (!MonsterClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SpawnSubsystem] %s의 몬스터 클래스가 비어 있어 제외함"), *RowName.ToString());
			continue;
		}

		for (int32 i = 0; i < Unit.Count; ++i)
		{
			PendingWaveMonsterClasses.Add(MonsterClass);
		}
	}

	TotalWaveMonsterCount = PendingWaveMonsterClasses.Num();
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

	PendingWaveMonsterClasses.Reset();
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

	const int32 RequestedCount = FMath::Min(ActiveWaveSpawnBatchSize, PendingWaveMonsterClasses.Num());
	TArray<FVector> Locations;
	if (RequestedCount <= 0 || !GameMode->FindSpawnLocation(RequestedCount, Locations))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	TArray<TSubclassOf<AASMonsterCharacter>> ClassesForAttempt = PendingWaveMonsterClasses;
	int32 SpawnedThisTick = 0;
	for (const FVector& Location : Locations)
	{
		if (ClassesForAttempt.IsEmpty())
		{
			break;
		}

		const int32 ClassIndex = FMath::RandRange(0, ClassesForAttempt.Num() - 1);
		const TSubclassOf<AASMonsterCharacter> MonsterClass = ClassesForAttempt[ClassIndex];
		ClassesForAttempt.RemoveAtSwap(ClassIndex);

		const FTransform SpawnTransform(FRotator::ZeroRotator, Location);
		if (World->SpawnActor<AASMonsterCharacter>(MonsterClass, SpawnTransform, SpawnParams))
		{
			PendingWaveMonsterClasses.RemoveSingleSwap(MonsterClass);
			++SpawnedThisTick;
		}
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
