#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectTypes.h"
#include "ASBossSpiderCeilingSlamAbility.generated.h"

class AASMonsterAttackIndicatorActor;
class AASMonsterCharacter;

/** 상승·추적·위치 확정·착지·Shockwave를 서버 C++로 실행하는 보스 스파이더 특수 패턴이다. */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASBossSpiderCeilingSlamAbility : public UASMonsterComboStepAbility
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
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Ceiling Slam|Cosmetic")
    void OnCeilingSlamRiseStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Ceiling Slam|Cosmetic")
    void OnCeilingSlamTrackingStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Ceiling Slam|Cosmetic")
    void OnCeilingSlamLandingTelegraph(const FVector& LockedLocation);

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Ceiling Slam|Cosmetic")
    void OnCeilingSlamLanded(const FVector& LandingLocation);

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Ceiling Slam|Cosmetic")
    void OnCeilingSlamShockwave(int32 ShockwaveIndex, const FVector& Origin, float Radius);

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Ceiling Slam|Cosmetic")
    void OnCeilingSlamFinished(bool bWasCancelled);

private:
    bool ResolveCeilingSlamConfig(FASBossSpiderCeilingSlamConfig& OutConfig, FASMonsterPatternEntry& OutPattern) const;
    bool IsTargetInActivationRange(const AASMonsterCharacter& Monster, const AActor& Target,
        const FASMonsterPatternEntry& Pattern) const;
    void BeginRise();
    void UpdateRise();
    void BeginTracking();
    void UpdateTracking();
    void BeginLandingTelegraph();
    void LandAndApplyImpact();
    void BeginNextShockwave();
    void BeginRecovery();
    void FinishCeilingSlam();
    void CancelCeilingSlam();
    void CleanupCeilingSlam(bool bWasCancelled);
    FVector ResolveGroundLocation(const FVector& CandidateLocation) const;
    /** Impact 중심은 유지하면서, 보스 Capsule만 겹친 플레이어 밖으로 C++에서 분리한다. */
    FVector ResolvePhysicalLandingLocation(const FVector& ImpactLocation) const;
    /** 추적 Indicator는 위치만 표시하고, 확정 뒤 Telegraph Indicator만 진행률 Fill을 사용한다. */
    void SpawnIndicator(float Duration, bool bUseVisualProgress);
    void DestroyIndicator();
    void ApplyRadialDamage(float Radius, float DamageMultiplier, bool bApplyStatus);
    FGameplayEffectSpecHandle MakeConfiguredDamageSpec(float DamageMultiplier) const;
    FGameplayEffectSpecHandle MakeConfiguredLandingStatusSpec() const;
    void ApplyCeilingUntargetableEffect();
    void RemoveCeilingUntargetableEffect();
    void DisableElevatedMeshShadow();
    void RestoreElevatedMeshShadow();
    void StopAIMovement();
    void RestoreAIFacing();
    void RestoreCharacterMovement();

    FASBossSpiderCeilingSlamConfig ActiveConfig;
    FASMonsterPatternEntry ActivePattern;
    TWeakObjectPtr<AActor> LockedTarget;
    TWeakObjectPtr<AASMonsterAttackIndicatorActor> ActiveIndicator;
    FVector RiseOrigin = FVector::ZeroVector;
    FVector LockedLandingLocation = FVector::ZeroVector;
    FVector LockedFacing = FVector::ForwardVector;
    double PhaseStartTime = 0.0;
    FTimerHandle RiseTimerHandle;
    FTimerHandle TrackingTimerHandle;
    FTimerHandle LandingTimerHandle;
    FTimerHandle ShockwaveTimerHandle;
    FTimerHandle RecoveryTimerHandle;
    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;
    uint8 PreviousCustomMovementMode = 0;
    FActiveGameplayEffectHandle CeilingUntargetableEffectHandle;
    int32 NextShockwaveIndex = 0;
    bool bHasActiveConfig = false;
    bool bMovementModeChanged = false;
    bool bIsElevated = false;
    bool bHasLanded = false;
    bool bPreviousMeshCastShadow = true;
    bool bMeshShadowStateCaptured = false;
    bool bCleanupComplete = true;
};
