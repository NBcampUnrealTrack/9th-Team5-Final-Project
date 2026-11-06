#pragma once

#include "CoreMinimal.h"
#include "Core/Spawn/ASMonsterSpawnBackend.h"
#include "Engine/DataTable.h"
#include "ASMonsterSpawnGroupData.generated.h"

class AASMonsterCharacter;
class UMassEntityConfigAsset;

/** 몬스터 그룹에 포함되는 특정 몬스터와 그 정확한 수량을 정의한다. */
USTRUCT(BlueprintType)
struct FASMonsterSpawnGroupUnitEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawn Group")
	EASMonsterSpawnBackend SpawnBackend = EASMonsterSpawnBackend::Actor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawn Group",
		meta = (EditCondition = "SpawnBackend == EASMonsterSpawnBackend::Actor", EditConditionHides))
	TSoftClassPtr<AASMonsterCharacter> MonsterClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawn Group",
		meta = (EditCondition = "SpawnBackend == EASMonsterSpawnBackend::Mass", EditConditionHides))
	TSoftObjectPtr<UMassEntityConfigAsset> MassEntityConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawn Group", meta = (ClampMin = "0"))
	int32 Count = 0;

	/**
	 * 스폰포인트 기준 상대 위치(cm). 스폰포인트의 회전을 따른다 (+X가 정면).
	 * i번째 개체는 LocalOffsets[i]에 생성되고, 오프셋이 없는 나머지 개체는 ScatterRadius 안에 무작위 배치된다.
	 * 낮 스폰(스폰포인트 기반)에만 적용되며 밤 웨이브는 무시한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawn Group")
	TArray<FVector> LocalOffsets;
};

/**
 * DT_MonsterSpawnGroup의 한 행. RowName과 무관하게 GroupIndex로 찾는다.
 * 어느 날짜에서 이 그룹을 뽑을지는 DT_DaySpawn이 결정한다.
 */
USTRUCT(BlueprintType)
struct FASMonsterSpawnGroupDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawn Group", meta = (ClampMin = "0"))
	int32 GroupIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawn Group")
	TArray<FASMonsterSpawnGroupUnitEntry> Units;
};
