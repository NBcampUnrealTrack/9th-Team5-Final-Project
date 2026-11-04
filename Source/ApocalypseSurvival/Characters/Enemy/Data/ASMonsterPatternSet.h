#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "StructUtils/InstancedStruct.h"
#include "ASMonsterPatternSet.generated.h"

/** CombatComponent가 일반 선택과 Director 요청을 어떻게 조합할지 결정한다. */
UENUM(BlueprintType)
enum class EASMonsterPatternSelectionPolicy : uint8
{
    WeightedRandom UMETA(DisplayName = "Weighted Random"),
    DirectorOnly UMETA(DisplayName = "Director Only"),
    Hybrid UMETA(DisplayName = "Hybrid")
};

/** Ability가 Commit 뒤 타깃 방향을 고정할지, 명시적으로 추적할지 결정한다. */
UENUM(BlueprintType)
enum class EASMonsterPatternFacingPolicy : uint8
{
    LockAtCommit UMETA(DisplayName = "Lock At Commit"),
    TrackTarget UMETA(DisplayName = "Track Target"),
    None UMETA(DisplayName = "None")
};

/** 보스별 Director/선택 정책 구조체가 파생할 수 있는 공통 베이스다. */
USTRUCT(BlueprintType)
struct FASMonsterPatternSelectionConfig
{
    GENERATED_BODY()
};

/** 각 PatternEntry의 RuntimeConfig가 파생할 수 있는 공통 베이스다. */
USTRUCT(BlueprintType)
struct FASMonsterPatternRuntimeConfig
{
    GENERATED_BODY()
};

/** PatternId 하나의 선택 규칙과 Native Ability Runtime 설정의 단일 원본이다. */
USTRUCT(BlueprintType)
struct FASMonsterPatternEntry
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (Categories = "Ability.Monster.Attack"))
    FGameplayTag PatternId;

    /** false면 부모에서 상속한 같은 PatternId도 명시적으로 비활성화한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern")
    bool bEnabled = true;

    /** 얇은 GA Blueprint Shell. Native C++ Ability의 자식만 지정한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (EditCondition = "bEnabled", EditConditionHides))
    TSubclassOf<UGameplayAbility> AbilityClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Range", meta = (ClampMin = "0.0", Units = "cm"))
    float MinRange = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Range", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxRange = 200.0f;

    /** 0이면 MaxRange만 사용한다. 0보다 크면 두 값 중 작은 값이 선택 사거리다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Range", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxGuaranteedTargetSurfaceDistance = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Selection", meta = (ClampMin = "0.0"))
    float SelectionWeight = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Selection", meta = (ClampMin = "0.0", Units = "s"))
    float Cooldown = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Selection")
    bool bCanStartDuringInterPatternDelay = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Selection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinOwnerHealthPercent = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Selection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaxOwnerHealthPercent = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Facing")
    EASMonsterPatternFacingPolicy FacingPolicy = EASMonsterPatternFacingPolicy::LockAtCommit;

    /** PatternId에 대응하는 타입 안전 C++ 설정 구조체. 자식 Asset은 같은 ID 전체를 교체한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Runtime", meta = (BaseStruct = "/Script/ApocalypseSurvival.ASMonsterPatternRuntimeConfig"))
    FInstancedStruct RuntimeConfig;
};

/** ParentPatternSet을 모두 적용한 뒤 런타임이 읽는 불변 스냅샷이다. */
USTRUCT()
struct FASResolvedMonsterPatternSet
{
    GENERATED_BODY()

    float InterPatternDelay = 0.0f;
    EASMonsterPatternSelectionPolicy SelectionPolicy = EASMonsterPatternSelectionPolicy::WeightedRandom;
    FInstancedStruct SelectionPolicyConfig;
    TArray<FASMonsterPatternEntry> PatternEntries;

    const FASMonsterPatternEntry* FindPattern(FGameplayTag PatternId) const;
};

/**
 * 모든 몬스터 패턴의 공통 Data Asset. 부모/자식 Asset의 직접 비교는 금지하고 Resolver API만 사용한다.
 * 같은 PatternId를 자식에 작성하면 Entry 전체를 교체한다. 이는 암묵적 필드 병합을 피하기 위한 규칙이다.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASMonsterPatternSet : public UDataAsset
{
    GENERATED_BODY()

public:
    /** 선택 사항. 순환 참조는 Data Validation과 런타임 Resolver에서 거부한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern Set|Inheritance")
    TObjectPtr<UASMonsterPatternSet> ParentPatternSet;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern Set|Timing")
    bool bOverrideInterPatternDelay = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern Set|Timing", meta = (EditCondition = "bOverrideInterPatternDelay", EditConditionHides, ClampMin = "0.0", Units = "s"))
    float InterPatternDelay = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern Set|Selection")
    bool bOverrideSelectionPolicy = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern Set|Selection", meta = (EditCondition = "bOverrideSelectionPolicy", EditConditionHides))
    EASMonsterPatternSelectionPolicy SelectionPolicy = EASMonsterPatternSelectionPolicy::WeightedRandom;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern Set|Selection")
    bool bOverrideSelectionPolicyConfig = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern Set|Selection", meta = (EditCondition = "bOverrideSelectionPolicyConfig", EditConditionHides, BaseStruct = "/Script/ApocalypseSurvival.ASMonsterPatternSelectionConfig"))
    FInstancedStruct SelectionPolicyConfig;

    /** 순서와 무관하다. 같은 Asset 안에서 PatternId 중복은 Validation 오류다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern Set|Patterns", meta = (TitleProperty = "PatternId"))
    TArray<FASMonsterPatternEntry> PatternEntries;

    /** 부모부터 명시적으로 병합해 실행 가능한 최종 PatternSet을 만든다. */
    bool ResolvePatternSet(FASResolvedMonsterPatternSet& OutResolved) const;

    /** 단일 PatternId의 최종 설정만 읽는 Ability/Director용 편의 API다. */
    bool ResolvePatternConfig(FGameplayTag PatternId, FASMonsterPatternEntry& OutConfig) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
    bool ResolvePatternSetInternal(FASResolvedMonsterPatternSet& OutResolved,
        TSet<const UASMonsterPatternSet*>& VisitingSets) const;
};
