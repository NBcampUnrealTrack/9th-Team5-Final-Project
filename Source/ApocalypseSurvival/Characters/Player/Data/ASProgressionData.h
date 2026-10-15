#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ASProgressionData.generated.h"

/*
 * 레벨, 경험치, 스킬포인트 규칙. 캐릭터 정의가 이 에셋을 가리킨다.
 * 기획 확정 전이라 값은 전부 데이터로 둔다.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASProgressionData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// 최대 레벨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level", meta = (ClampMin = "1"))
	int32 MaxLevel = 10;
	
	// [0] = 1->2 에 필요한 경험치, [1] = 2-> ... (모자라면 마지막 값을 계속 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
	TArray<int32> ExpToNextLevel;
	
	// 해당 레벨에 도달하면 받는 스킬포인트 (키: 레벨, 값: 포인트)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillPoint")
	TMap<int32, int32> SkillPointsAtLevel;
	
	// 현재 레벨에서 다음 레벨까지 필요한 경험치 (최대 레벨이면 0)
	int32 GetExpToNextLevel(int32 Level) const;
	
	// 해당 레벨 도달 시 받는 스킬포인트 (없으면 0)
	int32 GetSkillPointAtLevel(int32 Level) const;
};
