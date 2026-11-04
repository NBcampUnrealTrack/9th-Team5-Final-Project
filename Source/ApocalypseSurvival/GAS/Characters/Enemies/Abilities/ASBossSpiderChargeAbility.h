#pragma once

#include "CoreMinimal.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterChargeAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ASBossSpiderChargeAbility.generated.h"

class AASMonsterAttackIndicatorActor;
class AASMonsterCharacter;
class UActorComponent;

/**
 * 보스 스파이더의 서버 권한 돌진.
 * 이동, 충돌, 피해, 인디케이터 수명과 정리는 모두 C++가 소유한다.
 * Blueprint 자식은 사운드·VFX·Montage와 Indicator Visual Class만 제공한다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASBossSpiderChargeAbility : public UASMonsterChargeAbility
{
    GENERATED_BODY()

public:
    UASBossSpiderChargeAbility();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicatedEndAbility, bool bWasCancelled) override;

protected:
    /** CaptureCharge는 Director가 예약한 WebBound 타깃을 반환하도록 재정의한다. */
    virtual AActor* ResolveChargeTarget() const;

    /** Activate 전과 Windup 종료 시점에 타깃의 패턴별 전제 조건을 검증한다. */
    virtual bool IsChargeTargetValid(const AActor* Target) const;

    /** 현재 AbilitySpec의 PatternId로 PatternSet RuntimeConfig를 찾아 실행 중 쓸 스냅샷을 만든다. */
    virtual bool ResolveChargeConfig(FASBossSpiderChargeConfig& OutConfig) const;

private:
    void BeginChargeMovement();
    void TickChargeMovement();
    void BeginRecovery();
    void FinishCharge();
    void CancelCharge();
    void CleanupCharge(bool bWasCancelled);

    void ApplyLockedFacing();
    void RestoreAIFacing();
    void StopAIMovement();
    void RestoreCharacterMovement();
    void SuspendOverlapGuard(const FASBossSpiderChargeConfig& Config, AASMonsterCharacter& Monster);
    void RestoreOverlapGuard();
    void ResolveEndOverlap(const FASBossSpiderChargeConfig& Config, AASMonsterCharacter& Monster);

    bool ResolveTravelDistance(const FASBossSpiderChargeConfig& Config, AASMonsterCharacter& Monster,
        AActor& Target);
    bool ApplyInitialBlockerSweep(const FASBossSpiderChargeConfig& Config, AASMonsterCharacter& Monster);
    FVector ProjectChargeLocationToGround(const FVector& CandidateLocation,
        const FASBossSpiderChargeConfig& Config, const AASMonsterCharacter& Monster) const;

    void SpawnChargeIndicator(const FASBossSpiderChargeConfig& Config, AASMonsterCharacter& Monster);
    void DestroyChargeIndicator();
    void ApplyChargeDamage(const FASBossSpiderChargeConfig& Config, AASMonsterCharacter& Monster);
    FGameplayEffectSpecHandle MakeConfiguredDamageSpec(const FASBossSpiderChargeConfig& Config) const;
    FGameplayEffectSpecHandle MakeConfiguredStatusSpec(const FASBossSpiderChargeConfig& Config) const;

    TWeakObjectPtr<AActor> LockedTarget;
    FASBossSpiderChargeConfig ActiveConfig;
    bool bHasActiveConfig = false;
    FVector ChargeStartLocation = FVector::ZeroVector;
    FVector LockedChargeDirection = FVector::ForwardVector;
    float ResolvedTravelDistance = 0.0f;

    FASCombatTargetHitSet HitTargets;
    TWeakObjectPtr<AASMonsterAttackIndicatorActor> ActiveIndicator;

    FTimerHandle WindupTimerHandle;
    FTimerHandle MovementTimerHandle;
    FTimerHandle MovementTimeoutTimerHandle;
    FTimerHandle RecoveryTimerHandle;
    FTimerHandle FacingTimerHandle;

    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;
    uint8 PreviousCustomMovementMode = 0;
    bool bMovementModeChanged = false;
    bool bRecoveryStarted = false;
    bool bCleanupComplete = true;
    TWeakObjectPtr<UActorComponent> SuspendedOverlapGuard;
    bool bOverlapGuardTickWasEnabled = false;

protected:
    /** GA Blueprint Shell은 Cosmetic 작업만 수행한다. 반환값이나 Timer 흐름에는 관여하지 않는다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Charge|Cosmetic")
    void OnChargeWindupStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Charge|Cosmetic")
    void OnChargeMovementStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Charge|Cosmetic")
    void OnChargeRecoveryStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Charge|Cosmetic")
    void OnChargeFinished(bool bWasCancelled);
};
