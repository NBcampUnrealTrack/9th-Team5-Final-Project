#include "GAS/Characters/Enemies/Abilities/ASBossSpiderWebTrapAbility.h"

#include "AIController.h"
#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/BossSpider/ASBossSpiderWebTrap.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GAS/Characters/Enemies/Abilities/ASBossSpiderPresentationTags.h"
#include "GameFramework/Character.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
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
        || !ConfigSnapshot.TrapActorClass || !ConfigSnapshot.IndicatorClass || !ConfigSnapshot.WebBoundEffectClass
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
    bProjectilesLaunched = false;
    bProjectileFlightComplete = false;
    bRecoveryComplete = false;
    LockedTrapLocations.Reset();
    ActiveIndicators.Reset();
    StopAIMovement();
    ApplyLockedFacing();
    if (ActivePattern.FacingPolicy != EASMonsterPatternFacingPolicy::None)
    {
        Monster->GetWorldTimerManager().SetTimer(FacingTimerHandle, this, &ThisClass::ApplyLockedFacing, 0.02f, true);
    }

    StartWindupPresentation();
    OnWebTrapCastStarted();
    if (ActiveConfig.CastWindupTime <= 0.0f)
    {
        LaunchWebProjectiles();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(WindupTimerHandle, this, &ThisClass::LaunchWebProjectiles,
        ActiveConfig.CastWindupTime, false);
}

void UASBossSpiderWebTrapAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const bool bReplicatedEndAbility, const bool bWasCancelled)
{
    CleanupWebTrap(bWasCancelled);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASBossSpiderWebTrapAbility::StartWindupPresentation()
{
    PlayPresentationMontage();
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

void UASBossSpiderWebTrapAbility::LaunchWebProjectiles()
{
    if (!IsActive() || !bHasActiveConfig || bProjectilesLaunched)
    {
        return;
    }

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster) || !LockTargetLocations())
    {
        CancelWebTrap();
        return;
    }

    bProjectilesLaunched = true;
    SpawnFlightIndicators();

    FGameplayCueParameters CueParameters;
    CueParameters.Location = Monster->GetActorLocation();
    CueParameters.Normal = FVector::UpVector;
    CueParameters.RawMagnitude = static_cast<float>(LockedTrapLocations.Num());
    ExecutePresentationCue(ASBossSpiderPresentationTags::WebTrap_Cast, CueParameters);

    const float FlightTime = FMath::Max(0.0f, ActiveConfig.ProjectileFlightTime);
    if (FlightTime <= 0.0f)
    {
        SpawnWebTraps();
    }
    else
    {
        Monster->GetWorldTimerManager().SetTimer(ProjectileFlightTimerHandle, this,
            &ThisClass::SpawnWebTraps, FlightTime, false);
    }

    if (IsActive())
    {
        BeginRecovery();
    }
}

bool UASBossSpiderWebTrapAbility::LockTargetLocations()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    if (!IsValid(Monster) || !World)
    {
        return false;
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

    LockedTrapLocations.Reset();
    const int32 MaxTrapCount = FMath::Clamp(ActiveConfig.MaxTrapCount, 1, 3);
    for (AASPlayerCharacter* Player : Players)
    {
        if (LockedTrapLocations.Num() >= MaxTrapCount)
        {
            break;
        }

        FVector GroundLocation;
        if (ResolveGroundLocation(*Player, GroundLocation))
        {
            LockedTrapLocations.Add(GroundLocation);
        }
    }

    return !LockedTrapLocations.IsEmpty();
}

bool UASBossSpiderWebTrapAbility::ResolveGroundLocation(const AASPlayerCharacter& Player,
    FVector& OutLocation) const
{
    const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    if (!IsValid(Monster) || !World)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossSpiderWebTrapGround), false, Monster);
    QueryParams.AddIgnoredActor(&Player);
    const FVector PlayerLocation = Player.GetActorLocation();
    FHitResult FloorHit;
    if (!World->LineTraceSingleByChannel(FloorHit, PlayerLocation + FVector::UpVector * 1200.0f,
        PlayerLocation - FVector::UpVector * 2400.0f, ECC_WorldStatic, QueryParams)
        || FloorHit.ImpactNormal.Z < 0.5f)
    {
        return false;
    }

    OutLocation = FloorHit.ImpactPoint;
    return true;
}

void UASBossSpiderWebTrapAbility::SpawnFlightIndicators()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    if (!IsValid(Monster) || !World || !ActiveConfig.IndicatorClass)
    {
        return;
    }

    ActiveIndicators.Reset();
    for (const FVector& Location : LockedTrapLocations)
    {
        const FTransform SpawnTransform(FRotator::ZeroRotator, Location);
        AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
            ActiveConfig.IndicatorClass, SpawnTransform, Monster, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Indicator)
        {
            continue;
        }

        Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Circle, ActiveConfig.TrapRadius,
            0.0f, 0.0f, ActiveConfig.ProjectileFlightTime);
        Indicator->FinishSpawning(SpawnTransform);
        ActiveIndicators.Add(Indicator);
    }
}

void UASBossSpiderWebTrapAbility::DestroyFlightIndicators()
{
    for (const TWeakObjectPtr<AASMonsterAttackIndicatorActor>& IndicatorPtr : ActiveIndicators)
    {
        if (AASMonsterAttackIndicatorActor* Indicator = IndicatorPtr.Get())
        {
            Indicator->Destroy();
        }
    }
    ActiveIndicators.Reset();
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

    DestroyFlightIndicators();
    int32 SpawnedTrapCount = 0;
    for (const FVector& TrapLocation : LockedTrapLocations)
    {
        const FTransform SpawnTransform(FRotator::ZeroRotator, TrapLocation);
        AASBossSpiderWebTrap* Trap = World->SpawnActorDeferred<AASBossSpiderWebTrap>(ActiveConfig.TrapActorClass,
            SpawnTransform, Monster, Monster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Trap)
        {
            continue;
        }

        Trap->InitializeTrap(Monster, ActiveConfig);
        Trap->FinishSpawning(SpawnTransform);

        FGameplayCueParameters CueParameters;
        CueParameters.Location = TrapLocation;
        CueParameters.Normal = FVector::UpVector;
        CueParameters.RawMagnitude = ActiveConfig.TrapRadius;
        ExecutePresentationCue(ASBossSpiderPresentationTags::WebTrap_Spawn, CueParameters);
        ++SpawnedTrapCount;
    }

    OnWebTrapsSpawned(SpawnedTrapCount);
    bProjectileFlightComplete = true;
    TryFinishWebTrap();
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
        CompleteRecovery();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(RecoveryTimerHandle, this, &ThisClass::CompleteRecovery,
        ActiveConfig.RecoveryTime, false);
}

void UASBossSpiderWebTrapAbility::CompleteRecovery()
{
    if (!IsActive())
    {
        return;
    }

    bRecoveryComplete = true;
    TryFinishWebTrap();
}

void UASBossSpiderWebTrapAbility::TryFinishWebTrap()
{
    if (IsActive() && bProjectileFlightComplete && bRecoveryComplete)
    {
        FinishWebTrap();
    }
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
            World->GetTimerManager().ClearTimer(ProjectileFlightTimerHandle);
            World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
            World->GetTimerManager().ClearTimer(FacingTimerHandle);
        }
    }

    DestroyFlightIndicators();
    LockedTrapLocations.Reset();
    RestoreCharacterMovement();
    RestoreAIFacing();
    bHasActiveConfig = false;
    bProjectilesLaunched = false;
    bProjectileFlightComplete = false;
    bRecoveryComplete = false;
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
