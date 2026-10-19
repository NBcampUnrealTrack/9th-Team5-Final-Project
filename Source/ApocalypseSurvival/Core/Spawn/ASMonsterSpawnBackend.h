#pragma once

#include "CoreMinimal.h"
#include "ASMonsterSpawnBackend.generated.h"

/** 스폰 데이터 한 항목을 Actor 또는 Mass Entity 중 어느 런타임으로 생성할지 결정한다. */
UENUM(BlueprintType)
enum class EASMonsterSpawnBackend : uint8
{
	Actor,
	Mass
};
