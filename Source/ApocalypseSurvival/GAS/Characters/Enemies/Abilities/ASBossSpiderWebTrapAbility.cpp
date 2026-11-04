#include "GAS/Characters/Enemies/Abilities/ASBossSpiderWebTrapAbility.h"

#include "AIController.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/BossSpider/ASBossSpiderWebTrap.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayAbilitySpec.h"
#include "TimerManager.h"

namespace
{
    float GetWebTrapActorCapsuleRadius(const AActor& Actor)
    {
        const ACharacter* Character = Cast<ACharacter>(&Actor);
        const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
        return Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f;
    }
}

void UASBossSpiderWebTrapAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    if (!HasSkillAuthority())
    {
        FinishComboStep(true);
        return;
    }

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    const UASMonsterTargetingComponent* Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
    AActor* Target = Targeting ? Targeting->GetTargetActor() : nullptr;
    FASBossSpiderWebTrapConfig ConfigSnapshot;
    FASMonsterPatternEntry PatternSnapshot;
    if (!IsValid(Monster) || !IsValid(Target)
        || !ResolveWebTrapConfig(ConfigSnapshot, PatternSnapshot)
        || !ConfigSnapshot.TrapActorClass || !ConfigSnapshot.WebBoundEffectClass
        || !IsTargetInActivationRange(*Monster, *Target, PatternSnapshot))
    {
        FinishComboStep(true);
        return;
    }

    const FVector InitialFacing = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
    if (InitialFacing.IsNearlyZero() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        FinishComboStep(true);
        return;
    }

    ActiveConfig = ConfigSnapshot;
    ActivePattern = PatternSnapshot;
    LockedFacing = InitialFacing;
    bHasActiveConfig = true;
    bCleanupComplete = false;
    StopAIMovement();
    ApplyLockedFacing();
    if (ActivePattern.FacingPolicy != EASMonsterPatternFacingPolicy::None)
    {
        Monster->GetWorldTimerManager().SetTimer(FacingTimerHandle, this, &ThisClass::ApplyLockedFacing, 0.02f, true);
    }

    OnWebTrapCastStarted();
    if (ActiveConfig.CastWindupTime <= 0.0f)
    {
        SpawnWebTraps();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(WindupTimerHandle, this, &ThisClass::SpawnWebTraps,
        ActiveConfig.CastWindupTime, false);
}

void UASBossSpiderWebTrapAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const bool bReplicatedEndAbility, const bool bWasCancelled)
{
    CleanupWebTrap(bWasCancelled);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

bool UASBossSpiderWebTrapAbility::ResolveWebTrapConfig(FASBossSpiderWebTrapConfig& OutConfig,
    FASMonsterPatternEntry& OutPattern) const
{
    const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    const UASMonsterDefinition* Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
    const FGameplayAbilitySpec* CurrentSpec = GetCurrentAbilitySpec();
    if (!Definition || !Definition->PatternSet || !CurrentSpec)
    {
        return false;
    }

    for (const FGameplayTag& PatternId : CurrentSpec->DynamicAbilityTags)
    {
        FASMonsterPatternEntry Pattern;
        if (Definition->PatternSet->ResolvePatternConfig(PatternId, Pattern))
        {
            if (const FASBossSpiderWebTrapConfig* Config = Pattern.RuntimeConfig.GetPtr<FASBossSpiderWebTrapConfig>())
            {
                OutConfig = *Config;
                OutPattern = Pattern;
                return true;
            }
        }
    }
    return false;
}

bool UASBossSpiderWebTrapAbility::IsTargetInActivationRange(const AASMonsterCharacter& Monster,
    const AActor& Target, const FASMonsterPatternEntry& Pattern) const
{
    const float SurfaceDistance = FVector::Dist2D(Monster.GetActorLocation(), Target.GetActorLocation())
        - GetWebTrapActorCapsuleRadius(Monster) - GetWebTrapActorCapsuleRadius(Target);
    const float MinRange = FMath::Max(0.0f, Pattern.MinRange);
    float MaxRange = FMath::Max(MinRange, Pattern.MaxRange);
    if (Pattern.MaxGuaranteedTargetSurfaceDistance > 0.0f)
    {
        MaxRange = FMath::Min(MaxRange, Pattern.MaxGuaranteedTargetSurfaceDistance);
    }
    return SurfaceDistance >= MinRange && SurfaceDistance <= MaxRange;
}

void UASBossSpiderWebTrapAbility::SpawnWebTraps()
{
    if (!IsActive() || !bHasActiveConfig)
    {
        CancelWebTrap();
        return;
    }

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    if (!IsValid(Monster) || !World)
    {
        CancelWebTrap();
        return;
    }

    TArray<AASPlayerCharacter*> Players;
    for (TActorIterator<AASPlayerCharacter> It(World); It; ++It)
    {
        AASPlayerCharacter* Player = *It;
        if (IsValid(Player) && !Player->IsDead() && !Player->IsDowned())
        {
            Players.Add(Player);
        }
    }
    Players.Sort([Monster](const AASPlayerCharacter& Left, const AASPlayerCharacter& Right)
    {
        return FVector::DistSquared2D(Monster->GetActorLocation(), Left.GetActorLocation())
            < FVector::DistSquared2D(Monster->GetActorLocation(), Right.GetActorLocation());
    });

    int32 SpawnedTrapCount = 0;
    const int32 MaxTrapCount = FMath::Max(1, ActiveConfig.MaxTrapCount);
    for (AASPlayerCharacter* Player : Players)
    {
        if (SpawnedTrapCount >= MaxTrapCount)
        {
            break;
        }

        const FTransform SpawnTransform(FRotator::ZeroRotator, Player->GetActorLocation());
        AASBossSpiderWebTrap* Trap = World->SpawnActorDeferred<AASBossSpiderWebTrap>(ActiveConfig.TrapActorClass,
            SpawnTransform, Monster, Monster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Trap)
        {
            continue;
        }

        Trap->InitializeTrap(Monster, ActiveConfig);
        Trap->FinishSpawning(SpawnTransform);
        ++SpawnedTrapCount;
    }

    OnWebTrapsSpawned(SpawnedTrapCount);
    BeginRecovery();
}

void UASBossSpiderWebTrapAbility::BeginRecovery()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster) || !bHasActiveConfig)
    {
        CancelWebTrap();
        return;
    }

    if (ActiveConfig.RecoveryTime <= 0.0f)
    {
        FinishWebTrap();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(RecoveryTimerHandle, this, &ThisClass::FinishWebTrap,
        ActiveConfig.RecoveryTime, false);
}

void UASBossSpiderWebTrapAbility::FinishWebTrap()
{
    if (IsActive())
    {
        FinishComboStep(false);
    }
}

void UASBossSpiderWebTrapAbility::CancelWebTrap()
{
    if (IsActive())
    {
        FinishComboStep(true);
    }
}

void UASBossSpiderWebTrapAbility::CleanupWebTrap(const bool bWasCancelled)
{
    if (bCleanupComplete)
    {
        return;
    }
    bCleanupComplete = true;

    if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter()))
    {
        if (UWorld* World = Monster->GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindupTimerHandle);
            World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
            World->GetTimerManager().ClearTimer(FacingTimerHandle);
        }
    }

    RestoreCharacterMovement();
    RestoreAIFacing();
    bHasActiveConfig = false;
    OnWebTrapCastFinished(bWasCancelled);
}

void UASBossSpiderWebTrapAbility::ApplyLockedFacing()
{
    if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter()))
    {
        Monster->SetActorRotation(LockedFacing.Rotation());
    }
}

void UASBossSpiderWebTrapAbility::StopAIMovement()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster) || bMovementModeChanged)
    {
        return;
    }
    if (AAIController* Controller = Cast<AAIController>(Monster->GetController()))
    {
        Controller->StopMovement();
    }
    if (UCharacterMovementComponent* Movement = Monster->GetCharacterMovement())
    {
        PreviousMovementMode = Movement->MovementMode;
        PreviousCustomMovementMode = Movement->CustomMovementMode;
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
        bMovementModeChanged = true;
    }
}

void UASBossSpiderWebTrapAbility::RestoreAIFacing()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    AAIController* Controller = Monster ? Cast<AAIController>(Monster->GetController()) : nullptr;
    if (!Controller)
    {
        return;
    }
    UASMonsterTargetingComponent* Targeting = Monster->GetTargetingComponent();
    if (AActor* Target = Targeting ? Targeting->GetTargetActor() : nullptr)
    {
        Controller->SetFocus(Target);
    }
    else
    {
        Controller->ClearFocus(EAIFocusPriority::Gameplay);
    }
}

void UASBossSpiderWebTrapAbility::RestoreCharacterMovement()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UCharacterMovementComponent* Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
    if (Movement && bMovementModeChanged)
    {
        Movement->SetMovementMode(PreviousMovementMode, PreviousCustomMovementMode);
        bMovementModeChanged = false;
    }
}
