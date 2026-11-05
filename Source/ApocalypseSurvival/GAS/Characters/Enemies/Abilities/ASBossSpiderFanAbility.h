#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ASBossSpiderFanAbility.generated.h"

class AASMonsterAttackIndicatorActor;
class AASMonsterCharacter;

/**
 * 보스 스파이더의 전방 부채꼴 일반 패턴이다.
 * 범위·방향·예고·피해·후딜·취소 정리는 모두 서버 C++가 소유하고, BP 자식은 Cosmetic Event만 구현한다.
 */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASBossSpiderFanAbility : public UASMonsterComboStepAbility
{
    GENERATED_BODY()

public:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicatedEndAbility, bool bWasCancelled) override;

protected:
    virtual void StartWindupPresentation() override;
    virtual void StartActionPresentation() override;

    /** 마이그레이션 동안 유지하는 서버 전용 Legacy Event. 복제 연출은 Monster Presentation Helper를 사용한다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Fan|Cosmetic")
    void OnFanWindupStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Fan|Cosmetic")
    void OnFanStrike();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Fan|Cosmetic")
    void OnFanFinished(bool bWasCancelled);

    /** Fan Montage가 시작할 Windup Section. 비어 있으면 Montage 처음부터 재생한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Fan|Presentation")
    FName WindupMontageSection = TEXT("WindupStart");

    /** Native 타격 시점에 전환할 Action Section. 없거나 비어 있으면 전환을 건너뛴다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Fan|Presentation")
    FName ActionMontageSection = TEXT("Action");

private:
    bool ResolveFanConfig(FASBossSpiderFanConfig& OutConfig, FASMonsterPatternEntry& OutPattern) const;
    bool IsTargetInActivationRange(const AASMonsterCharacter& Monster, const AActor& Target,
        const FASMonsterPatternEntry& Pattern) const;
    void BeginFanStrike();
    void ApplyFanStrikeEffect();
    void BeginRecovery();
    void FinishFan();
    void CancelFan();
    void CleanupFan(bool bWasCancelled);
    void UpdateFacing();
    FVector ResolveLockedAttackOrigin(const AASMonsterCharacter& Monster) const;
    void StopAIMovement();
    void RestoreAIFacing();
    void RestoreCharacterMovement();
    void SpawnFanIndicator();
    void DestroyFanIndicator();
    void ApplyFanDamage();
    FGameplayEffectSpecHandle MakeConfiguredDamageSpec() const;
    FGameplayEffectSpecHandle MakeConfiguredStatusSpec() const;

    FASBossSpiderFanConfig ActiveConfig;
    FASMonsterPatternEntry ActivePattern;
    TWeakObjectPtr<AActor> LockedTarget;
    TWeakObjectPtr<AASMonsterAttackIndicatorActor> ActiveIndicator;
    FVector LockedFacing = FVector::ForwardVector;
    FVector LockedAttackOrigin = FVector::ZeroVector;
    FTimerHandle WindupTimerHandle;
    FTimerHandle ActionHitTimerHandle;
    FTimerHandle ActionDurationTimerHandle;
    FTimerHandle RecoveryTimerHandle;
    FTimerHandle FacingTimerHandle;
    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;
    uint8 PreviousCustomMovementMode = 0;
    bool bHasActiveConfig = false;
    bool bMovementModeChanged = false;
    bool bCleanupComplete = true;
    bool bStrikeEffectApplied = false;
};
