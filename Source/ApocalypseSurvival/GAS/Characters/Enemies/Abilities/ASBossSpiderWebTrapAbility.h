#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ASBossSpiderWebTrapAbility.generated.h"

class AASMonsterCharacter;

/** 보스가 살아 있는 플레이어 위치에 C++ 복제 WebTrap을 생성하는 Director 전용 패턴이다. */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASBossSpiderWebTrapAbility : public UASMonsterComboStepAbility
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
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Web Trap|Cosmetic")
    void OnWebTrapCastStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Web Trap|Cosmetic")
    void OnWebTrapsSpawned(int32 SpawnedTrapCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Web Trap|Cosmetic")
    void OnWebTrapCastFinished(bool bWasCancelled);

private:
    bool ResolveWebTrapConfig(FASBossSpiderWebTrapConfig& OutConfig, FASMonsterPatternEntry& OutPattern) const;
    bool IsTargetInActivationRange(const AASMonsterCharacter& Monster, const AActor& Target,
        const FASMonsterPatternEntry& Pattern) const;
    void SpawnWebTraps();
    void BeginRecovery();
    void FinishWebTrap();
    void CancelWebTrap();
    void CleanupWebTrap(bool bWasCancelled);
    void ApplyLockedFacing();
    void StopAIMovement();
    void RestoreAIFacing();
    void RestoreCharacterMovement();

    FASBossSpiderWebTrapConfig ActiveConfig;
    FASMonsterPatternEntry ActivePattern;
    FVector LockedFacing = FVector::ForwardVector;
    FTimerHandle WindupTimerHandle;
    FTimerHandle RecoveryTimerHandle;
    FTimerHandle FacingTimerHandle;
    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;
    uint8 PreviousCustomMovementMode = 0;
    bool bHasActiveConfig = false;
    bool bMovementModeChanged = false;
    bool bCleanupComplete = true;
};
