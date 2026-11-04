#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ASBossSpiderEggSummonAbility.generated.h"

class AASMonsterCharacter;

/** PatternSet의 EggSummon 설정만 읽어 서버에서 Native Egg Actor를 생성하는 보스 전용 Ability다. */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASBossSpiderEggSummonAbility : public UASMonsterComboStepAbility
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
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Egg Summon|Cosmetic")
    void OnEggSummonStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Egg Summon|Cosmetic")
    void OnEggsSpawned(int32 SpawnedEggCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Egg Summon|Cosmetic")
    void OnEggSummonFinished(bool bWasCancelled);

private:
    bool ResolveEggSummonConfig(FASBossSpiderEggSummonConfig& OutConfig, FASMonsterPatternEntry& OutPattern) const;
    bool IsTargetInActivationRange(const AASMonsterCharacter& Monster, const AActor& Target,
        const FASMonsterPatternEntry& Pattern) const;
    int32 CalculateEggCount() const;
    bool FindEggSpawnLocation(const FVector& AnchorLocation, const TArray<FVector>& ExistingLocations,
        FVector& OutLocation) const;
    void SpawnEggs();
    void BeginRecovery();
    void FinishEggSummon();
    void CancelEggSummon();
    void CleanupEggSummon(bool bWasCancelled);
    void ApplyLockedFacing();
    void StopAIMovement();
    void RestoreAIFacing();
    void RestoreCharacterMovement();

    FASBossSpiderEggSummonConfig ActiveConfig;
    FASMonsterPatternEntry ActivePattern;
    FVector LockedFacing = FVector::ForwardVector;
    FVector LockedTargetLocation = FVector::ZeroVector;
    FTimerHandle WindupTimerHandle;
    FTimerHandle RecoveryTimerHandle;
    FTimerHandle FacingTimerHandle;
    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;
    uint8 PreviousCustomMovementMode = 0;
    bool bHasActiveConfig = false;
    bool bMovementModeChanged = false;
    bool bCleanupComplete = true;
};
