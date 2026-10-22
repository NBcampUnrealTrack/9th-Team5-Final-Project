#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "ASMonsterStatDefinition.generated.h"

/** 현재 몬스터 전투 코드가 공통으로 소비하는 기본 스탯 태그다. */
namespace ASMonsterStatTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxHealth);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(MoveSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackPower);
}

/** 수치가 계산 파이프라인의 어느 단계에 들어갈지 지정한다. */
UENUM(BlueprintType)
enum class EASMonsterStatModifierOperation : uint8
{
	/** 라운드/난이도 성장 뒤, 퍼센트 계산 전에 실제 값을 더한다. 예: 체력 +20. */
	PreFlat,

	/** 같은 계층끼리 합산한 뒤 한 번 적용한다. 0.20은 +20%, -0.30은 -30%다. */
	PercentAdd,

	/** 독립된 최종 배율을 순서대로 곱한다. 1.20은 x1.2, 0.70은 x0.7이다. */
	PercentMult,

	/** 모든 퍼센트와 독립 배율 계산이 끝난 뒤 실제 값을 더한다. 예: 최종 피해 +10. */
	PostFlat,

	/** 앞 단계의 계산값을 지정값으로 교체한다. ChannelPriority와 Priority가 가장 높은 값이 승리한다. */
	Override,
};

/** 스탯 하나의 기본값과 마지막 안전 제한이다. */
USTRUCT(BlueprintType)
struct FASMonsterStatEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	FGameplayTag StatTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	float BaseValue = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	float MinValue = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	float MaxValue = TNumericLimits<float>::Max();

	/** 1.0이면 정수, 0.01이면 소수 둘째 자리까지 최종값을 고정한다. 0 이하는 양자화하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat", meta = (ClampMin = "0.0"))
	float QuantizationStep = 0.0f;
};

/** 몬스터와 스탯별 성장 곡선. Curve가 비어 있으면 해당 성장 계층을 적용하지 않는다. */
USTRUCT(BlueprintType)
struct FASMonsterStatGrowthRule
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth")
	FGameplayTag StatTag;

	/** X는 라운드 번호, Y는 배율이다. 낮과 밤은 같은 라운드 번호를 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth")
	TObjectPtr<UCurveFloat> RoundMultiplierCurve;

	/** X는 게임 난이도 숫자, Y는 배율이다. 현재 난이도 시스템이 없으면 1.0을 전달한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth")
	TObjectPtr<UCurveFloat> DifficultyMultiplierCurve;
};

/** 버프, 디버프, 이벤트가 Resolver에 전달하는 단위다. Source 단위 제거와 디버그에 쓴다. */
USTRUCT(BlueprintType)
struct FASMonsterStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	EASMonsterStatModifierOperation Operation = EASMonsterStatModifierOperation::PercentAdd;

	/** 독립 계산 계층의 정렬 순서. 값이 낮은 Channel부터 적용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	int32 ChannelPriority = 0;

	/** 같은 Channel에서 Override 충돌을 결정한다. 높을수록 우선한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	int32 Priority = 0;

	/** Pre/PostFlat과 Override는 실제 값, PercentAdd는 0.2 = +20%, PercentMult는 1.2 = x1.2다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	float Magnitude = 0.0f;

	/** 같은 효과 묶음을 정확히 제거할 수 있도록 부여자가 지정하는 이름이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	FName Source = NAME_None;
};

/** 이번 스폰 또는 재계산에만 유효한 외부 입력값이다. */
USTRUCT(BlueprintType)
struct FASMonsterStatResolveContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Context", meta = (ClampMin = "1"))
	int32 Round = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Context", meta = (ClampMin = "0.0"))
	float Difficulty = 1.0f;
};

/** Resolver가 계산을 마친 뒤 Actor와 Mass 어댑터가 읽는 값 묶음이다. */
USTRUCT(BlueprintType)
struct FASResolvedMonsterStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	TMap<FGameplayTag, float> Values;

	float GetValue(const FGameplayTag& StatTag, const float FallbackValue) const;
	void SetValue(const FGameplayTag& StatTag, float Value);
};

/**
 * 몬스터의 기본 스탯과 선택적 성장 곡선을 한 곳에 둔다.
 * Actor와 Mass는 같은 자산과 Resolver를 공유하고, 계산 결과를 적용하는 저장소만 다르게 쓴다.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASMonsterStatDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UASMonsterStatDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (TitleProperty = "StatTag"))
	TArray<FASMonsterStatEntry> Stats;

	/** 비어 있으면 이 Definition은 고정 스탯이며 라운드 또는 난이도에 성장하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth", meta = (TitleProperty = "StatTag"))
	TArray<FASMonsterStatGrowthRule> GrowthRules;

	const FASMonsterStatEntry* FindStat(const FGameplayTag& StatTag) const;
	const FASMonsterStatGrowthRule* FindGrowthRule(const FGameplayTag& StatTag) const;
};

/** 공통 계산 규칙. GAS와 Mass가 같은 수치를 얻도록 순수 데이터만 입력으로 받는다. */
struct APOCALYPSESURVIVAL_API FASMonsterStatResolver
{
	static FASResolvedMonsterStats Resolve(const UASMonsterStatDefinition& Definition,
		const FASMonsterStatResolveContext& Context,
		TConstArrayView<FASMonsterStatModifier> Modifiers = {});

	static float ResolveValue(const FASMonsterStatEntry& Entry,
		const FASMonsterStatGrowthRule* GrowthRule,
		const FASMonsterStatResolveContext& Context,
		TConstArrayView<FASMonsterStatModifier> Modifiers);
};
