#pragma once

#include "CoreMinimal.h"
#include "Core/Spawn/ASMonsterSpawnBackend.h"
#include "Engine/DataTable.h"
#include "ASWaveData.generated.h"

class AASMonsterCharacter;
class UMassEntityConfigAsset;

/** 밤 웨이브가 공통 몬스터 그룹을 몇 번 반복해 편성할지 정의한다. */
USTRUCT(BlueprintType)
struct FASWaveSpawnGroupEntry
{
	GENERATED_BODY()

	/** DT_MonsterSpawnGroup의 GroupIndex. 낮 스폰과 같은 그룹 구성을 재사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0"))
	int32 MonsterGroupIndex = 0;

	/** 그룹 안의 모든 Unit을 이 횟수만큼 웨이브에 추가한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "1"))
	int32 RepeatCount = 1;
};

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

	/**
	 * 밤 웨이브가 사용할 공통 몬스터 그룹 목록이다.
	 * 그룹의 Unit 구성은 DT_MonsterSpawnGroup에만 정의해 낮/밤 데이터 중복을 막는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (TitleProperty = "MonsterGroupIndex"))
	TArray<FASWaveSpawnGroupEntry> MonsterGroups;

	/**
	 * 이전 DT_WaveData 행 호환용 직접 유닛 목록이다.
	 * 새 행은 MonsterGroups만 사용한다. MonsterGroups가 비어 있을 때에만 fallback으로 읽는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave|Deprecated", AdvancedDisplay,
		meta = (DeprecatedProperty, DeprecationMessage = "Use MonsterGroups and DT_MonsterSpawnGroup instead."))
	TArray<FASWaveUnitEntry> Units;
};
