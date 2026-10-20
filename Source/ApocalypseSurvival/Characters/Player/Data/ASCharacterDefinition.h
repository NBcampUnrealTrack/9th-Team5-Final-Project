#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AttributeSet.h"
#include "ASCharacterDefinition.generated.h"

class AASPlayerCharacter;
class UTexture2D;
class UASAbilitySet;
class UASProgressionData;

/* 조작 난이도 (선택 화면 표시용) */
UENUM(BlueprintType)
enum class EASCharacterDifficulty : uint8
{
	Low UMETA(DisplayName = "낮음"),
	Normal UMETA(DisplayName = "보통"),
	High UMETA(DisplayName = "높음")
};

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

/* 선택 화면에 보여 줄 능력, 스킬 한 줄 (표시용) */
USTRUCT(BlueprintType)
struct FASSkillDisplayEntry
{
	GENERATED_BODY()
	
	// 분류 (예: 고유 특성, 액티브, 패시브, 궁극기)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText Category;
	
	// 키 (QWER)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText Key;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText Name;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (MultiLine = true))
	FText Description;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;
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
	
	// 칭호 (ex 초가속 격투자)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText Title;
	
	// 능력명 (ex 신체 시간 가속)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText AbilityName;
	
	// 역할 (ex 브루저)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText Role;
	
	// 전투 방식 (ex 근거리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText CombatStyle;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	EASCharacterDifficulty Difficulty = EASCharacterDifficulty::Normal;
	
	// 대표 무기 (표시용 문구)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText RepresentativeWeapons;
	
	// 선택 화면 오른쪽 목록 (고유 특성 QWER 순서로 넣는다.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (TitleProperty = "Name"))
	TArray<FASSkillDisplayEntry> SkillDisplays;
	
	// 이 캐릭터로 스폰할 BP
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TSubclassOf<AASPlayerCharacter> CharacterClass;
	
	/*
	 * 이 캐릭터 전용 어빌리티. (스킬)
	 * 공통 어빌리티 (평타, 구르기, 사망 등)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<const UASAbilitySet> CharacterAbilitySet;
	
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
