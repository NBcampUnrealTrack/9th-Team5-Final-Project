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
    /** GA Blueprint Shell의 Montage, SFX, VFX만 이 Event에서 재생한다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Fan|Cosmetic")
    void OnFanWindupStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Fan|Cosmetic")
    void OnFanStrike();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Fan|Cosmetic")
    void OnFanFinished(bool bWasCancelled);

private:
    bool ResolveFanConfig(FASBossSpiderFanConfig& OutConfig, FASMonsterPatternEntry& OutPattern) const;
    bool IsTargetInActivationRange(const AASMonsterCharacter& Monster, const AActor& Target,
        const FASMonsterPatternEntry& Pattern) const;
    void BeginFanStrike();
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
    FTimerHandle RecoveryTimerHandle;
    FTimerHandle FacingTimerHandle;
    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;
    uint8 PreviousCustomMovementMode = 0;
    bool bHasActiveConfig = false;
    bool bMovementModeChanged = false;
    bool bCleanupComplete = true;
};
