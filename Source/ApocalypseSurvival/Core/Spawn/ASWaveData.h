#pragma once

#include "CoreMinimal.h"
#include "Core/Spawn/ASMonsterSpawnBackend.h"
#include "Engine/DataTable.h"
#include "ASWaveData.generated.h"

class AASMonsterCharacter;
class UMassEntityConfigAsset;

/** 웨이브에서 특정 몬스터를 정확히 몇 마리 생성할지 정의한다. */
USTRUCT(BlueprintType)
struct FASWaveUnitEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	EASMonsterSpawnBackend SpawnBackend = EASMonsterSpawnBackend::Actor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave",
		meta = (EditCondition = "SpawnBackend == EASMonsterSpawnBackend::Actor", EditConditionHides))
	TSoftClassPtr<AASMonsterCharacter> MonsterClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave",
		meta = (EditCondition = "SpawnBackend == EASMonsterSpawnBackend::Mass", EditConditionHides))
	TSoftObjectPtr<UMassEntityConfigAsset> MassEntityConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0"))
	int32 Count = 0;
};

/** DataTable 한 행에 대응하는 스웜 웨이브 설정. 행 이름은 Wave_01, Wave_02처럼 사용한다. */
USTRUCT(BlueprintType)
struct FASWaveDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "1"))
	int32 SpawnBatchSize = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.01"))
	float SpawnInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	TArray<FASWaveUnitEntry> Units;
};
