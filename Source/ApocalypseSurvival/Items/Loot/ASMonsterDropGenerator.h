#pragma once

#include "CoreMinimal.h"
#include "Items/Loot/ASLootGenerator.h"

struct FASMonsterDropGroupRow;

/** 몬스터 사망 보상용 아이템 추첨기. 일반 상자 추첨과 독립된 규칙을 둔다. */
class APOCALYPSESURVIVAL_API FASMonsterDropGenerator : public FASLootGenerator
{
public:
	static bool Generate(const FASMonsterDropGroupRow& DropGroup, FRandomStream& Random,
		TArray<FASItemGrant>& OutItems, FString& OutError);
};
