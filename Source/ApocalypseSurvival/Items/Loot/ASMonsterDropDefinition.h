#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Items/Data/ASItemGrant.h"
#include "Items/Loot/ASLootDefinition.h"
#include "ASMonsterDropDefinition.generated.h"

/**
 * 몬스터 하나가 사망할 때 참조하는 드랍 그룹이다.
 * 상자 프리셋의 외형·상자 클래스 설정과 분리해 아이템 추첨 규칙만 보관한다.
 */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASMonsterDropGroupRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 그룹 자체가 발동할 확률. 실패하면 어떤 아이템도 생성하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropChance = 1.0f;

	/** 확률과 관계없이 항상 추가하는 아이템. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop|Fixed")
	TArray<FASItemGrant> FixedItems;

	/** 가중치 기반 랜덤 후보. 기존 FASLootPoolEntry DataTable을 재사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop|Random",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/ApocalypseSurvival.ASLootPoolEntry"))
	TObjectPtr<UDataTable> RandomPool;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop|Random", meta = (ClampMin = "0", ClampMax = "10000"))
	int32 MinRollCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop|Random", meta = (ClampMin = "0", ClampMax = "10000"))
	int32 MaxRollCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop|Random")
	TMap<EASItemType, float> CategoryWeights;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop|Random")
	TArray<FASLootTagWeight> TagWeights;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop|Random")
	bool bAllowRepeatedItems = true;

	/** 같은 아이템이 여러 번 뽑히면 하나의 월드 스택으로 합친다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	bool bMergeIdenticalItems = true;

	bool ResolveRandomEntries(TArray<FASLootPoolEntry>& OutEntries, FString& OutError) const;
	bool Validate(FString& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
