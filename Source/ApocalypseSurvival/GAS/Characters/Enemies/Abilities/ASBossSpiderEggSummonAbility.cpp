#include "GAS/Characters/Enemies/Abilities/ASBossSpiderEggSummonAbility.h"

#include "AIController.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/BossSpider/ASBossSpiderEgg.h"
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
    float GetEggSummonActorCapsuleRadius(const AActor& Actor)
    {
        const ACharacter* Character = Cast<ACharacter>(&Actor);
        const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
        return Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f;
    }
}

void UASBossSpiderEggSummonAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
    FASBossSpiderEggSummonConfig ConfigSnapshot;
    FASMonsterPatternEntry PatternSnapshot;
    if (!IsValid(Monster) || !IsValid(Target)
        || !ResolveEggSummonConfig(ConfigSnapshot, PatternSnapshot)
        || !ConfigSnapshot.EggActorClass || !ConfigSnapshot.EliteMinionClass
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
    LockedTargetLocation = Target->GetActorLocation();
    bHasActiveConfig = true;
    bCleanupComplete = false;
    bEggsSpawned = false;
    StopAIMovement();
    ApplyLockedFacing();
    if (ActivePattern.FacingPolicy != EASMonsterPatternFacingPolicy::None)
    {
        Monster->GetWorldTimerManager().SetTimer(FacingTimerHandle, this, &ThisClass::ApplyLockedFacing, 0.02f, true);
    }

    StartWindupPresentation();
    OnEggSummonStarted();
    if (ActiveConfig.CastWindupTime <= 0.0f)
    {
        BeginEggSummonAction();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(WindupTimerHandle, this, &ThisClass::BeginEggSummonAction,
        ActiveConfig.CastWindupTime, false);
}

void UASBossSpiderEggSummonAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const bool bReplicatedEndAbility, const bool bWasCancelled)
{
    CleanupEggSummon(bWasCancelled);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASBossSpiderEggSummonAbility::StartWindupPresentation()
{
    PlayPresentationMontage(WindupMontageSection);
}

void UASBossSpiderEggSummonAbility::StartActionPresentation()
{
    JumpPresentationMontageSection(ActionMontageSection);
}

void UASBossSpiderEggSummonAbility::StartRecoveryPresentation()
{
    JumpPresentationMontageSection(RecoveryMontageSection);
}

bool UASBossSpiderEggSummonAbility::ResolveEggSummonConfig(FASBossSpiderEggSummonConfig& OutConfig,
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
            if (const FASBossSpiderEggSummonConfig* Config = Pattern.RuntimeConfig.GetPtr<FASBossSpiderEggSummonConfig>())
            {
                OutConfig = *Config;
                OutPattern = Pattern;
                return true;
            }
        }
    }
    return false;
}

bool UASBossSpiderEggSummonAbility::IsTargetInActivationRange(const AASMonsterCharacter& Monster,
    const AActor& Target, const FASMonsterPatternEntry& Pattern) const
{
    const float SurfaceDistance = FVector::Dist2D(Monster.GetActorLocation(), Target.GetActorLocation())
        - GetEggSummonActorCapsuleRadius(Monster) - GetEggSummonActorCapsuleRadius(Target);
    const float MinRange = FMath::Max(0.0f, Pattern.MinRange);
    float MaxRange = FMath::Max(MinRange, Pattern.MaxRange);
    if (Pattern.MaxGuaranteedTargetSurfaceDistance > 0.0f)
    {
        MaxRange = FMath::Min(MaxRange, Pattern.MaxGuaranteedTargetSurfaceDistance);
    }
    return SurfaceDistance >= MinRange && SurfaceDistance <= MaxRange;
}

int32 UASBossSpiderEggSummonAbility::CalculateEggCount() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return 0;
    }

    int32 AlivePlayerCount = 0;
    for (TActorIterator<AASPlayerCharacter> It(World); It; ++It)
    {
        const AASPlayerCharacter* Player = *It;
        if (IsValid(Player) && !Player->IsDead() && !Player->IsDowned())
        {
            ++AlivePlayerCount;
        }
    }

    const int32 AdditionalPlayers = FMath::Max(0, AlivePlayerCount - 1);
    const int32 DesiredCount = ActiveConfig.BaseEggCount
        + AdditionalPlayers * FMath::Max(0, ActiveConfig.AdditionalEggsPerAlivePlayer);
    return FMath::Clamp(DesiredCount, 1, FMath::Clamp(ActiveConfig.MaxEggCount, 1, 6));
}

bool UASBossSpiderEggSummonAbility::FindEggSpawnLocation(const FVector& AnchorLocation,
    const TArray<FVector>& ExistingLocations, FVector& OutLocation) const
{
    UWorld* World = GetWorld();
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!World || !IsValid(Monster))
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossSpiderEggSpawn), false, Monster);
    const float MinRadius = FMath::Max(0.0f, ActiveConfig.SpawnMinRadius);
    const float MaxRadius = FMath::Max(MinRadius, ActiveConfig.SpawnMaxRadius);
    const float MinSpacingSquared = FMath::Square(FMath::Max(1.0f, ActiveConfig.MinEggSpacing));
    for (int32 Attempt = 0; Attempt < 20; ++Attempt)
    {
        const float AngleRadians = FMath::FRandRange(0.0f, 2.0f * PI);
        const float Radius = FMath::FRandRange(MinRadius, MaxRadius);
        FVector Candidate = AnchorLocation + FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f) * Radius;
        FHitResult FloorHit;
        if (!World->LineTraceSingleByChannel(FloorHit, Candidate + FVector(0.0f, 0.0f, 1200.0f),
            Candidate - FVector(0.0f, 0.0f, 2400.0f), ECC_WorldStatic, QueryParams)
            || FloorHit.ImpactNormal.Z < 0.5f)
        {
            continue;
        }

        Candidate = FloorHit.ImpactPoint;
        bool bTooClose = false;
        for (const FVector& ExistingLocation : ExistingLocations)
        {
            if (FVector::DistSquared2D(Candidate, ExistingLocation) < MinSpacingSquared)
            {
                bTooClose = true;
                break;
            }
        }
        if (!bTooClose)
        {
            OutLocation = Candidate;
            return true;
        }
    }
    return false;
}

void UASBossSpiderEggSummonAbility::BeginEggSummonAction()
{
    if (!IsActive() || !bHasActiveConfig)
    {
        CancelEggSummon();
        return;
    }

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster))
    {
        CancelEggSummon();
        return;
    }

    StartActionPresentation();
    const float ActionDuration = FMath::Max(0.0f, ActiveConfig.ActionDuration);
    const float SpawnDelay = FMath::Clamp(ActiveConfig.ActionSpawnDelay, 0.0f, ActionDuration);
    if (SpawnDelay <= 0.0f)
    {
        SpawnEggs();
    }
    else
    {
        Monster->GetWorldTimerManager().SetTimer(ActionSpawnTimerHandle, this,
            &ThisClass::SpawnEggs, SpawnDelay, false);
    }

    if (ActionDuration <= 0.0f)
    {
        BeginRecovery();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(ActionDurationTimerHandle, this,
        &ThisClass::BeginRecovery, ActionDuration, false);
}

void UASBossSpiderEggSummonAbility::SpawnEggs()
{
    if (!IsActive() || !bHasActiveConfig || bEggsSpawned)
    {
        return;
    }

    bEggsSpawned = true;

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    if (!IsValid(Monster) || !World)
    {
        CancelEggSummon();
        return;
    }

    const int32 DesiredCount = CalculateEggCount();
    TArray<FVector> SpawnLocations;
    SpawnLocations.Reserve(DesiredCount);
    for (int32 EggIndex = 0; EggIndex < DesiredCount; ++EggIndex)
    {
        FVector SpawnLocation;
        if (FindEggSpawnLocation(LockedTargetLocation, SpawnLocations, SpawnLocation))
        {
            SpawnLocations.Add(SpawnLocation);
        }
    }

    int32 SpawnedEggCount = 0;
    for (const FVector& SpawnLocation : SpawnLocations)
    {
        const FTransform SpawnTransform(LockedFacing.Rotation(), SpawnLocation);
        AASBossSpiderEgg* Egg = World->SpawnActorDeferred<AASBossSpiderEgg>(ActiveConfig.EggActorClass,
            SpawnTransform, Monster, Monster, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Egg)
        {
            continue;
        }

        Egg->InitializeEgg(Monster, ActiveConfig);
        Egg->FinishSpawning(SpawnTransform);
        ++SpawnedEggCount;
    }

    OnEggsSpawned(SpawnedEggCount);
    FGameplayCueParameters CueParameters;
    CueParameters.Location = Monster->GetActorLocation();
    CueParameters.Normal = LockedFacing;
    CueParameters.RawMagnitude = static_cast<float>(SpawnedEggCount);
    ExecutePresentationCue(ASBossSpiderPresentationTags::Egg_Spawn, CueParameters);
}

void UASBossSpiderEggSummonAbility::BeginRecovery()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster) || !bHasActiveConfig)
    {
        CancelEggSummon();
        return;
    }

    StartRecoveryPresentation();

    if (ActiveConfig.RecoveryTime <= 0.0f)
    {
        FinishEggSummon();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(RecoveryTimerHandle, this, &ThisClass::FinishEggSummon,
        ActiveConfig.RecoveryTime, false);
}

void UASBossSpiderEggSummonAbility::FinishEggSummon()
{
    if (IsActive())
    {
        FinishComboStep(false);
    }
}

void UASBossSpiderEggSummonAbility::CancelEggSummon()
{
    if (IsActive())
    {
        FinishComboStep(true);
    }
}

void UASBossSpiderEggSummonAbility::CleanupEggSummon(const bool bWasCancelled)
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
            World->GetTimerManager().ClearTimer(ActionSpawnTimerHandle);
            World->GetTimerManager().ClearTimer(ActionDurationTimerHandle);
            World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
            World->GetTimerManager().ClearTimer(FacingTimerHandle);
        }
    }

    RestoreCharacterMovement();
    RestoreAIFacing();
    bHasActiveConfig = false;
    bEggsSpawned = false;
    OnEggSummonFinished(bWasCancelled);
}

void UASBossSpiderEggSummonAbility::ApplyLockedFacing()
{
    if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter()))
    {
        Monster->SetActorRotation(LockedFacing.Rotation());
    }
}

void UASBossSpiderEggSummonAbility::StopAIMovement()
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

void UASBossSpiderEggSummonAbility::RestoreAIFacing()
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

void UASBossSpiderEggSummonAbility::RestoreCharacterMovement()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UCharacterMovementComponent* Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
    if (Movement && bMovementModeChanged)
    {
        Movement->SetMovementMode(PreviousMovementMode, PreviousCustomMovementMode);
        bMovementModeChanged = false;
    }
}
