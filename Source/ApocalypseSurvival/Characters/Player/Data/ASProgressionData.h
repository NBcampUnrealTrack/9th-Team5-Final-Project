#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ASProgressionData.generated.h"

/*
 * 특정 레벨에 열리는 스킬 강화 단계 하나 (히오스식: 해당 슬롯 강화 중 하나를 고른다)
 * 선택지 내용과 강화 효과는 스킬 담당 데이터. 여기서는 "언제, 어느 슬롯, 몇 개 중"만 정한다.
 */
USTRUCT(BlueprintType)
struct FASSkillUpgradeTier
{
	GENERATED_BODY()
	
	// 이 레벨에 도달하면 열린다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillUpgrade", meta = (ClampMin = "2"))
	int32 Level = 2;
	
	// 강화할 스킬 슬롯 (InputTag.Skill.Q / W / E / R)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillUpgrade", meta = (Categories = "InputTag.Skill"))
	FGameplayTag SkillSlotTag;
	
	// 고를 수 있는 선택지 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillUpgrade", meta = (ClampMin = "1"))
	int32 ChoiceCount = 3;
};

/*
 * 레벨, 경험치, 스킬 강화 단계 규칙. 캐릭터 정의가 이 에셋을 가리킨다.
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
	
	// [0] = 1->2 에 필요한 경험치, [1] = 2->3 ... (모자라면 마지막 값을 계속 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
	TArray<int32> ExpToNextLevel;
	
	// 스킬 강화 단계 (레벨 오름차순, 같은 레벨 중복 불가). 배열 번호 = 단계 번호
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillUpgrade")
	TArray<FASSkillUpgradeTier> SkillUpgradeTiers;
	
	// 현재 레벨에서 다음 레벨까지 필요한 경험치 (최대 레벨이면 0)
	int32 GetExpToNextLevel(int32 Level) const;
	
	// 해당 레벨에 열리는 단계 번호 (없으면 INDEX_NONE)
	int32 FindSkillUpgradeTierIndex(int32 Level) const;
	
	// 해당 레벨까지 열린 단계 수 (오름차순이라 0 ~ 개수-1 번 단계가 열린 것)
	int32 GetUnlockedSkillUpgradeTierCount(int32 Level) const;
	
#if WITH_EDITOR
	// 순서, 중복, 최대 레벨 초과, 빈 슬롯 경고
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};