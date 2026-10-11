#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ASDaySpawnData.generated.h"

/** 날짜 스폰에서 선택할 몬스터 그룹 후보와 상대 가중치. */
USTRUCT(BlueprintType)
struct FASDayMonsterGroupSelectionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Spawn", meta = (ClampMin = "0"))
	int32 MonsterGroupIndex = 0;

	/** 이 그룹이 선택될 상대 확률. 0이면 후보에서 제외한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Spawn", meta = (ClampMin = "0.0"))
	float GroupWeight = 1.0f;
};

/** 날짜별 필드 몬스터 배치 규칙. 행 이름은 Day_01, Day_02처럼 사용한다. */
USTRUCT(BlueprintType)
struct FASDaySpawnDataRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 랜덤으로 선택할 스폰포인트 수. 선택된 포인트마다 몬스터 그룹 하나를 가중치로 뽑는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Spawn", meta = (ClampMin = "0"))
	int32 SpawnCount = 0;

	/** 각 스폰포인트에서 선택할 몬스터 그룹 후보. 유닛 구성은 DT_MonsterSpawnGroup에서 읽는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Spawn")
	TArray<FASDayMonsterGroupSelectionEntry> MonsterGroupSelections;
};
