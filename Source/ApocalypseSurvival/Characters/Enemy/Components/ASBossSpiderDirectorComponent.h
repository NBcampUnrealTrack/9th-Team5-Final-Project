#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ASBossSpiderDirectorComponent.generated.h"

class AASMonsterCharacter;
class UASMonsterPatternSet;
class UASMonsterCombatComponent;

/**
 * 거미 여왕 전용 보스 패턴 지휘자.
 *
 * 일반 평타와 일반 돌진은 StateTree/CombatComponent의 가중치 선택에 남긴다. 반대로 알 소환,
 * 천장 낙하, 거미줄 포획 뒤 확정 돌진처럼 전투 문맥이 필요한 패턴은 이 컴포넌트가 PatternId를
 * 지정해서 실행한다. 서버에서만 판단하며, PatternId와 쿨다운은 Resolved PatternSet이
 * 최종 권한을 가진다.
 */
UCLASS(ClassGroup = (Enemy), BlueprintType, meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASBossSpiderDirectorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UASBossSpiderDirectorComponent();

    /** 거미줄 장판이 플레이어를 완전히 속박했을 때 호출한다. 다음 가능한 틱에 확정 돌진을 시도한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Boss Spider")
    void SetCaptureTarget(AActor* NewCaptureTarget);

    /** 이미 다른 플레이어가 속박된 경우 false를 반환해 완전 속박 대상을 한 명으로 제한한다. */
    bool TrySetCaptureTarget(AActor* NewCaptureTarget);

    /** 대상이 탈출하거나 사망했을 때 호출해 확정 돌진 예약을 제거한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Boss Spider")
    void ClearCaptureTarget(AActor* ExpectedTarget = nullptr);

    /** 살아 있는 보스 알이 하나 이상이면 true. 알이 남아 있는 동안 알 소환을 재시도하지 않는다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Boss Spider")
    void SetEggsActive(bool bInEggsActive);

    UFUNCTION(BlueprintPure, Category = "Enemy|Boss Spider")
    bool HasActiveEggs() const;

    /** Native Egg Actor가 등록/해제한다. 여러 알 중 하나만 남아도 다음 EggSummon을 막는다. */
    void RegisterActiveEgg(AActor* EggActor);
    void UnregisterActiveEgg(AActor* EggActor);

    /** 포획 돌진 Ability가 사용할 현재 서버 권한 속박 대상을 반환한다. */
    AActor* GetCaptureTarget() const { return CaptureTarget.Get(); }

    /** MonsterDefinition에서만 가져오는 단일 Gameplay 설정 자산. */
    const UASMonsterPatternSet* GetPatternSet() const;

    /** 특수 PatternId를 즉시 요청하는 Blueprint용 진입점. 성공하면 종료 후딜도 Director가 정리한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Boss Spider")
    bool TryRequestSpecialPattern(FGameplayTag PatternId, AActor* Target);

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    AASMonsterCharacter* GetBossMonster() const;
    UASMonsterCombatComponent* GetCombatComponent() const;
    bool TryStartDirectorPattern(FGameplayTag PatternId, AActor* Target);
    void FinishDirectorPatternIfNeeded();
    double GetWorldTimeSeconds() const;

    /**
     * 기존 BP_BossSpider 초안의 직렬화 보존용 필드다. 런타임은 MonsterDefinition->PatternSet만 읽는다.
     * DA_PatternSet_BossSpider 이관 검증 뒤에만 제거한다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Boss Spider|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Set MonsterDefinition->PatternSet instead."))
    TObjectPtr<UASBossSpiderPatternSet> PatternSet;

    UPROPERTY(EditDefaultsOnly, Category = "Boss Spider|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Set FASBossSpiderDirectorConfig in PatternSet instead."))
    float DecisionInterval = 0.15f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss Spider|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Set FASBossSpiderDirectorConfig in PatternSet instead."))
    float PhaseTwoHealthPercent = 0.60f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss Spider|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Set FASBossSpiderDirectorConfig in PatternSet instead."))
    float MajorPatternInterval = 13.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss Spider|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Set FASBossSpiderDirectorConfig in PatternSet instead."))
    float WebTrapInterval = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss Spider|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Set FASBossSpiderDirectorConfig in PatternSet instead."))
    FGameplayTag WebTrapPatternId;

    UPROPERTY(EditDefaultsOnly, Category = "Boss Spider|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Set FASBossSpiderDirectorConfig in PatternSet instead."))
    FGameplayTag EggSummonPatternId;

    UPROPERTY(EditDefaultsOnly, Category = "Boss Spider|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Set FASBossSpiderDirectorConfig in PatternSet instead."))
    FGameplayTag CeilingSlamPatternId;

    UPROPERTY(EditDefaultsOnly, Category = "Boss Spider|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Set FASBossSpiderDirectorConfig in PatternSet instead."))
    FGameplayTag CaptureChargePatternId;

    TWeakObjectPtr<AActor> CaptureTarget;
    FASBossSpiderDirectorConfig ActiveConfig;
    bool bHasActiveConfig = false;
    /** 기존 BP 알 로직과의 직렬화 호환용 상태다. Native Egg는 ActiveEggActors를 사용한다. */
    bool bLegacyEggsActive = false;
    TSet<TWeakObjectPtr<AActor>> ActiveEggActors;
    bool bWaitingForDirectorPattern = false;
    /** 2페이즈 진입 직후에는 천장 낙하로 시작하고, 그다음 알 소환과 번갈아 시도한다. */
    bool bNextMajorPatternPrefersEggs = false;
    double NextDecisionTime = 0.0;
    double NextMajorPatternTime = 0.0;
    double NextWebTrapTime = 0.0;
};
