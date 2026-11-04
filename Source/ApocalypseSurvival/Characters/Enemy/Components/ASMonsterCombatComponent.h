#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASMonsterPatternSet.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "ASMonsterCombatComponent.generated.h"

class UASAbilitySystemComponent;

/**
 * 공격 사거리를 확인하고, 공격 Ability 의 활성화를 태그로 요청한다.
 */
UCLASS(ClassGroup = (Enemy), BlueprintType, meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASMonsterCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UASMonsterCombatComponent();

    void Configure(float InAttackRange, FGameplayTag InAttackAbilityTag);

    /** Resolver가 확정한 PatternSet과 그 순서에 대응하는 Ability Spec Handle을 등록한다. */
    void ConfigurePatterns(const TArray<FASMonsterPatternEntry>& InPatterns,
        const TArray<FGameplayAbilitySpecHandle>& InAbilityHandles,
        EASMonsterPatternSelectionPolicy InSelectionPolicy = EASMonsterPatternSelectionPolicy::WeightedRandom);

    /** 기존 Blueprint Telegraph 래퍼의 직렬화 호환용이다. 새 PatternSet Ability는 사용하지 않는다. */
    void ConfigureLegacyTelegraphPatterns(const TArray<FASMonsterAttackPattern>& InPatterns);

    /** 보스 전용 Director가 일반 패턴 사이의 공통 대기 시간을 서버에서 설정한다. */
    void SetInterPatternDelay(float InInterPatternDelay);

    UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
    float GetAttackRange() const { return AttackRange; }

    /**
     * 가중치 기반 일반 선택에 참여하는 Pattern의 유효 최대 표면 사거리다.
     * false면 Director 전용 Pattern뿐이므로 기존 Definition 기반 이동 거리를 유지한다.
     */
    bool TryGetAutonomousPatternApproachRange(float& OutRange) const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
    FGameplayTag GetAttackAbilityTag() const { return AttackAbilityTag; }

    /** 패턴 목록을 사용 중인지. 비어 있으면 기존 단일 공격 경로를 유지한다. */
    UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
    bool UsesAttackPatterns() const { return !AttackPatterns.IsEmpty(); }

    /** 현재 몬스터의 체력 비율 (0.0 ~ 1.0)을 반환한다. */
    UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
    float GetHealthPercent() const;

    /** 두 캡슐 표면 사이의 수평 거리가 사거리 안이면 true. 높이 차이는 무시한다. */
    UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
    bool IsTargetInAttackRange(const AActor* Target) const;

    /** 현재 거리·패턴 쿨다운·공용 후딜 기준으로 공격을 요청할 수 있는지 검사한다. */
    UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
    bool CanRequestAttack(const AActor* Target) const;

    /**
     * 서버 전용. 타깃이 사거리 안이면 공격 Ability 활성화를 요청한다.
     * 반환값은 Ability 가 실제로 시작됐는지 여부다.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Combat")
    bool TryRequestAttack(AActor* Target);

    /**
     * 서버 전용. PatternId에 해당하는 한 가지 패턴이 현재 실행 가능한지 검사한다.
     * SelectionWeight가 0인 패턴도 검사한다. 보스 Director가 랜덤 후보에서 제외한 특수기를
     * 정해진 전투 상황에만 실행할 때 사용한다.
     */
    UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
    bool CanRequestPatternById(FGameplayTag PatternId, const AActor* Target) const;

    /**
     * 서버 전용. PatternId에 해당하는 패턴 하나만 활성화한다.
     * 일반 TryRequestAttack과 달리 가중치 추첨을 하지 않으며, SelectionWeight가 0이어도 실행 가능하다.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Combat")
    bool TryRequestPatternAttackById(FGameplayTag PatternId, AActor* Target);

    /** 패턴 Ability가 아직 실행 중인지. 즉발 패턴은 활성화 직후 false가 될 수 있다. */
    bool IsPatternAttackActive() const;

    /** StateTree가 패턴 Ability 종료를 확인했을 때 호출한다. 공용 후딜은 이 시점부터 시작한다. */
    void NotifyPatternAttackFinished();

    // --- 장판 표시 및 타격 래퍼 함수 선언 ---
    void ShowTelegraphForPattern(int32 PatternIndex);
    void HideTelegraph();
    void ApplyPatternDamage(int32 PatternIndex);

private:
    UASAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
    bool IsPatternInRange(const FASMonsterPatternEntry& Pattern, const AActor* Target) const;
    bool IsPatternReady(int32 PatternIndex, double CurrentTime, bool bRequireSelectionWeight = true) const;
    int32 FindPatternIndexById(FGameplayTag PatternId) const;
    bool CanRequestPatternAtIndex(int32 PatternIndex, const AActor* Target, bool bRequireSelectionWeight) const;
    bool TryRequestPatternAttackAtIndex(int32 PatternIndex, AActor* Target, bool bRequireSelectionWeight);
    bool TryRequestPatternAttack(AActor* Target);

    double GetWorldTimeSeconds() const;

    UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
    float AttackRange = 180.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (Categories = "Ability.Monster"))
    FGameplayTag AttackAbilityTag;

    /** 하나의 패턴 Ability가 끝난 뒤, 다음 일반 패턴까지 기다리는 공통 숨 고르기 시간. */
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Pattern", meta = (ClampMin = "0.0", Units = "s"))
    float PostAttackRecovery = 0.4f;

    EASMonsterPatternSelectionPolicy PatternSelectionPolicy = EASMonsterPatternSelectionPolicy::WeightedRandom;

    struct FPatternRuntime
    {
       FGameplayAbilitySpecHandle AbilityHandle;
       double CooldownEndTime = 0.0;
    };

    TArray<FASMonsterPatternEntry> AttackPatterns;
    TArray<FPatternRuntime> PatternRuntime;
    TArray<FASMonsterAttackPattern> LegacyTelegraphPatterns;
    double NextAttackAllowedTime = 0.0;
    bool bWaitingForPatternAttackEnd = false;

    // --- 장판 인디케이터 관리용 변수 ---
    UPROPERTY(Transient)
    TObjectPtr<class AASMonsterAttackIndicatorActor> CurrentTelegraphActor;
};
