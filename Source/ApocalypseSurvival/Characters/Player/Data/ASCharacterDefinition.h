#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AttributeSet.h"
#include "ASCharacterDefinition.generated.h"

class AASPlayerCharacter;
class UTexture2D;
class UASProgressionData;

/* 레벨 1일 때 어트리뷰트 기본값 하나 */
USTRUCT(BlueprintType)
struct FASAttributeInitEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttribute Attribute;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute")
	float Value = 0.f;
};

/* 레벨 1 오를때 마다 고정으로 늘어나는 어트리뷰트 하나 */
USTRUCT(BlueprintType)
struct FASAttributeGrowEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth")
	FGameplayAttribute Attribute;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth")
	float ValuePerLevel = 0.f;
};

/*
 * 캐릭터 1종의 고정 데이터 (기본 스탯, 레벨당 성장)
 * 메시, 애니메이션, 어빌리티 세트는 CharacterClass(캐릭터 BP)에서 지정
 */

UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASCharacterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// 선택 화면 표시용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (MultiLine = true))
	FText Description;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;
	
	// 이 캐릭터로 스폰할 BP
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TSubclassOf<AASPlayerCharacter> CharacterClass;
	
	// 레벨 1 기준 어트리뷰트 값 (MaxHealth와 Health처럼 최대, 현재 둘 다 넣는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes")
	TArray<FASAttributeInitEntry> BaseAttributes;
	
	// 레벨업마다 오르는 값 (Max 계열과 공격력, 이동속도 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes")
	TArray<FASAttributeGrowEntry> LevelGrowth;
	
	// 레벨별 필요 경험치, 최대레벨, 스킬포인트 규칙
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	TObjectPtr<const UASProgressionData> ProgressionData;
	
	// 레벨 1 기본값 (목록에 없으면 DefaultValue)
	float GetBaseAttributeValue(const FGameplayAttribute& Attribute, float DefaultValue = 0.f) const;
	
	// 레벨당 증가량 (목록에 없으면 0)
	float GetGrowthPerLevel(const FGameplayAttribute& Attribute) const;
	
	// 해당 레벨의 값 = 기본값 + 레벨당 증가량 * (Level - 1)
	float GetAttributeValueAtLevel(const FGameplayAttribute& Attribute, int32 Level, float DefaultValue = 0.f) const;
	
#if WITH_EDITOR
	// 저장, 검증 시 잘못된 데이터 경고 (빈 칸, 중복)
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
