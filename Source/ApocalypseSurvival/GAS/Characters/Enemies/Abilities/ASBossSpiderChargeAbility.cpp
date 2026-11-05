#include "GAS/Characters/Enemies/Abilities/ASBossSpiderChargeAbility.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "Characters/Enemy/Data/ASMonsterPatternSet.h"
#include "Components/ActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Skills/ASSkillTags.h"
#include "TimerManager.h"

UASBossSpiderChargeAbility::UASBossSpiderChargeAbility()
{
    bStopMovementWhileCasting = false;
    bUseTargetDistance = false;
    WindupPresentationMontage.StartSection = TEXT("WindupStart");
    ActionPresentationMontage.StartSection = TEXT("Launch");
    RecoveryPresentationMontage.StartSection = TEXT("Recovery");
}

void UASBossSpiderChargeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    if (!HasSkillAuthority())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    AActor* Target = ResolveChargeTarget();
    FASBossSpiderChargeConfig ConfigSnapshot;
    if (!IsValid(Monster) || !IsValid(Target) || !IsChargeTargetValid(Target) || !ResolveChargeConfig(ConfigSnapshot))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ActiveConfig = ConfigSnapshot;
    bHasActiveConfig = true;
    const FASBossSpiderChargeConfig* Config = &ActiveConfig;

    LockedTarget = Target;
    ChargeStartLocation = Monster->GetActorLocation();
    LockedChargeDirection = (Target->GetActorLocation() - ChargeStartLocation).GetSafeNormal2D();
    if (LockedChargeDirection.IsNearlyZero()
        || !ResolveTravelDistance(*Config, *Monster, *Target)
        || !ApplyInitialBlockerSweep(*Config, *Monster))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    bCleanupComplete = false;
    HitTargets.Reset();
    bRecoveryStarted = false;
    ApplyLockedFacing();
    Monster->GetWorldTimerManager().SetTimer(FacingTimerHandle, this, &UASBossSpiderChargeAbility::ApplyLockedFacing,
        FMath::Max(0.005f, Config->StepInterval), true);

    SpawnChargeIndicator(*Config, *Monster);
    StartWindupPresentation();
    OnChargeWindupStarted();
    if (Config->WindupTime <= 0.0f)
    {
        BeginChargeMovement();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(WindupTimerHandle, this,
        &UASBossSpiderChargeAbility::BeginChargeMovement, Config->WindupTime, false);
}

void UASBossSpiderChargeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const bool bReplicatedEndAbility, const bool bWasCancelled)
{
    CleanupCharge(bWasCancelled);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASBossSpiderChargeAbility::StartWindupPresentation()
{
    PlayPresentationMontage(WindupPresentationMontage);
}

void UASBossSpiderChargeAbility::StartActionPresentation()
{
    if (!PlayPresentationMontage(ActionPresentationMontage))
    {
        StopPresentationMontage();
    }
}

void UASBossSpiderChargeAbility::StartRecoveryPresentation()
{
    if (!PlayPresentationMontage(RecoveryPresentationMontage))
    {
        StopPresentationMontage();
    }
}

AActor* UASBossSpiderChargeAbility::ResolveChargeTarget() const
{
    const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    const UASMonsterTargetingComponent* Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
    return Targeting ? Targeting->GetTargetActor() : nullptr;
}

bool UASBossSpiderChargeAbility::IsChargeTargetValid(const AActor* Target) const
{
    return IsValid(Target);
}

bool UASBossSpiderChargeAbility::ResolveChargeConfig(FASBossSpiderChargeConfig& OutConfig) const
{
    const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    const UASMonsterDefinition* Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
    if (!Definition || !Definition->PatternSet)
    {
        return false;
    }

    const FGameplayAbilitySpec* CurrentSpec = GetCurrentAbilitySpec();
    if (!CurrentSpec)
    {
        return false;
    }

    for (const FGameplayTag& PatternId : CurrentSpec->DynamicAbilityTags)
    {
        FASMonsterPatternEntry PatternEntry;
        if (Definition->PatternSet->ResolvePatternConfig(PatternId, PatternEntry))
        {
            if (const FASBossSpiderChargeConfig* Config = PatternEntry.RuntimeConfig.GetPtr<FASBossSpiderChargeConfig>())
            {
                OutConfig = *Config;
                return true;
            }
        }
    }

    return false;
}

void UASBossSpiderChargeAbility::BeginChargeMovement()
{
    if (!IsActive())
    {
        return;
    }

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    const FASBossSpiderChargeConfig* Config = bHasActiveConfig ? &ActiveConfig : nullptr;
    if (!IsValid(Monster) || !Config || !IsChargeTargetValid(LockedTarget.Get()))
    {
        CancelCharge();
        return;
    }

    StopAIMovement();
    SuspendOverlapGuard(*Config, *Monster);
    DestroyChargeIndicator();
    Monster->ForceNetUpdate();
    StartActionPresentation();

    if (Config->MovementStartDelay <= 0.0f)
    {
        StartChargeMovement();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(MovementStartDelayTimerHandle, this,
        &UASBossSpiderChargeAbility::StartChargeMovement, Config->MovementStartDelay, false);
}

void UASBossSpiderChargeAbility::StartChargeMovement()
{
    if (!IsActive())
    {
        return;
    }

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    const FASBossSpiderChargeConfig* Config = bHasActiveConfig ? &ActiveConfig : nullptr;
    if (!IsValid(Monster) || !Config || !IsChargeTargetValid(LockedTarget.Get()))
    {
        CancelCharge();
        return;
    }

    OnChargeMovementStarted();
    Monster->GetWorldTimerManager().SetTimer(MovementTimerHandle, this,
        &UASBossSpiderChargeAbility::TickChargeMovement, FMath::Max(0.005f, Config->StepInterval), true);
    const float SafeMoveSpeed = FMath::Max(1.0f, Config->MoveSpeed);
    const float MovementTimeout = (ResolvedTravelDistance / SafeMoveSpeed)
        + FMath::Max(Config->MovementTimeoutGrace, Config->StepInterval * 2.0f);
    Monster->GetWorldTimerManager().SetTimer(MovementTimeoutTimerHandle, this,
        &UASBossSpiderChargeAbility::CancelCharge, MovementTimeout, false);
}

void UASBossSpiderChargeAbility::TickChargeMovement()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    const FASBossSpiderChargeConfig* Config = bHasActiveConfig ? &ActiveConfig : nullptr;
    if (!IsActive() || !IsValid(Monster) || !Config)
    {
        CancelCharge();
        return;
    }

    const FVector CurrentLocation = Monster->GetActorLocation();
    const float TravelledDistance = FVector::DotProduct(CurrentLocation - ChargeStartLocation, LockedChargeDirection);
    const float RemainingDistance = ResolvedTravelDistance - TravelledDistance;
    if (RemainingDistance <= FMath::Max(0.0f, Config->CompletionTolerance))
    {
        BeginRecovery();
        return;
    }

    const float StepDistance = FMath::Min(Config->MoveSpeed * Config->StepInterval, RemainingDistance);
    FVector NextLocation = CurrentLocation + LockedChargeDirection * StepDistance;

    // 일반 충돌 Sweep은 사용하지 않는다. 플레이어, 알, 몬스터, 트랩을 통과해야 한다.
    NextLocation = ProjectChargeLocationToGround(NextLocation, *Config, *Monster);
    Monster->SetActorLocation(NextLocation, false, nullptr, ETeleportType::TeleportPhysics);
    ApplyLockedFacing();
    ApplyChargeDamage(*Config, *Monster);
}

void UASBossSpiderChargeAbility::BeginRecovery()
{
    if (bRecoveryStarted)
    {
        return;
    }

    bRecoveryStarted = true;
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    const FASBossSpiderChargeConfig* Config = bHasActiveConfig ? &ActiveConfig : nullptr;
    if (!IsValid(Monster) || !Config)
    {
        CancelCharge();
        return;
    }

    Monster->GetWorldTimerManager().ClearTimer(MovementTimerHandle);
    Monster->GetWorldTimerManager().ClearTimer(MovementTimeoutTimerHandle);
    ResolveEndOverlap(*Config, *Monster);
    Monster->ForceNetUpdate();
    StartRecoveryPresentation();
    OnChargeRecoveryStarted();
    if (Config->RecoveryTime <= 0.0f)
    {
        FinishCharge();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(RecoveryTimerHandle, this,
        &UASBossSpiderChargeAbility::FinishCharge, Config->RecoveryTime, false);
}

void UASBossSpiderChargeAbility::FinishCharge()
{
    if (IsActive())
    {
        FinishComboStep(false);
    }
}

void UASBossSpiderChargeAbility::CancelCharge()
{
    if (IsActive())
    {
        FinishComboStep(true);
    }
}

void UASBossSpiderChargeAbility::CleanupCharge(const bool bWasCancelled)
{
    if (bCleanupComplete)
    {
        return;
    }
    bCleanupComplete = true;

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (IsValid(Monster))
    {
        UWorld* World = Monster->GetWorld();
        if (World)
        {
            World->GetTimerManager().ClearTimer(WindupTimerHandle);
            World->GetTimerManager().ClearTimer(MovementStartDelayTimerHandle);
            World->GetTimerManager().ClearTimer(MovementTimerHandle);
            World->GetTimerManager().ClearTimer(MovementTimeoutTimerHandle);
            World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
            World->GetTimerManager().ClearTimer(FacingTimerHandle);
        }
    }

    DestroyChargeIndicator();
    RestoreOverlapGuard();
    RestoreCharacterMovement();
    RestoreAIFacing();
    LockedTarget.Reset();
    HitTargets.Reset();
    ResolvedTravelDistance = 0.0f;
    bHasActiveConfig = false;
    bRecoveryStarted = false;
    OnChargeFinished(bWasCancelled);
}

void UASBossSpiderChargeAbility::ApplyLockedFacing()
{
    if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter()))
    {
        Monster->SetActorRotation(LockedChargeDirection.Rotation());
    }
}

void UASBossSpiderChargeAbility::RestoreAIFacing()
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

void UASBossSpiderChargeAbility::StopAIMovement()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!Monster || bMovementModeChanged)
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

void UASBossSpiderChargeAbility::RestoreCharacterMovement()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UCharacterMovementComponent* Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
    if (!Movement || !bMovementModeChanged)
    {
        return;
    }

    Movement->SetMovementMode(PreviousMovementMode, PreviousCustomMovementMode);
    bMovementModeChanged = false;
}

void UASBossSpiderChargeAbility::SuspendOverlapGuard(const FASBossSpiderChargeConfig& Config,
    AASMonsterCharacter& Monster)
{
    if (Config.OverlapGuardComponentName.IsNone() || SuspendedOverlapGuard.IsValid())
    {
        return;
    }

    TInlineComponentArray<UActorComponent*> Components(&Monster);
    for (UActorComponent* Component : Components)
    {
        if (Component && Component->GetFName() == Config.OverlapGuardComponentName)
        {
            bOverlapGuardTickWasEnabled = Component->IsComponentTickEnabled();
            Component->SetComponentTickEnabled(false);
            SuspendedOverlapGuard = Component;
            return;
        }
    }
}

void UASBossSpiderChargeAbility::RestoreOverlapGuard()
{
    if (UActorComponent* Guard = SuspendedOverlapGuard.Get())
    {
        Guard->SetComponentTickEnabled(bOverlapGuardTickWasEnabled);
    }

    SuspendedOverlapGuard.Reset();
    bOverlapGuardTickWasEnabled = false;
}

bool UASBossSpiderChargeAbility::ResolveTravelDistance(const FASBossSpiderChargeConfig& Config,
    AASMonsterCharacter& Monster, AActor& Target)
{
    const float CenterDistance = FVector::Dist2D(Monster.GetActorLocation(), Target.GetActorLocation());
    const float MinimumDistance = FMath::Max(0.0f, Config.MinTravelDistance);
    const float MaximumDistance = FMath::Max(MinimumDistance, Config.MaxTravelDistance);
    ResolvedTravelDistance = FMath::Clamp(CenterDistance + FMath::Max(0.0f, Config.TargetOvershootDistance),
        MinimumDistance, MaximumDistance);
    return ResolvedTravelDistance > 0.0f;
}

bool UASBossSpiderChargeAbility::ApplyInitialBlockerSweep(const FASBossSpiderChargeConfig& Config,
    AASMonsterCharacter& Monster)
{
    UWorld* World = Monster.GetWorld();
    UCapsuleComponent* Capsule = Monster.GetCapsuleComponent();
    if (!World || !Capsule)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossSpiderChargeBlocker), false, &Monster);
    QueryParams.AddIgnoredActor(&Monster);

    FHitResult Hit;
    const FVector EndLocation = ChargeStartLocation + LockedChargeDirection * ResolvedTravelDistance;
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
        Capsule->GetScaledCapsuleHalfHeight());
    if (World->SweepSingleByChannel(Hit, ChargeStartLocation, EndLocation, FQuat::Identity,
        Config.BlockerTraceChannel, Shape, QueryParams))
    {
        ResolvedTravelDistance *= FMath::Clamp(Hit.Time, 0.0f, 1.0f);
    }

    return ResolvedTravelDistance > FMath::Max(0.0f, Config.CompletionTolerance);
}

void UASBossSpiderChargeAbility::ResolveEndOverlap(const FASBossSpiderChargeConfig& Config,
    AASMonsterCharacter& Monster)
{
    AActor* Target = LockedTarget.Get();
    UCapsuleComponent* MonsterCapsule = Monster.GetCapsuleComponent();
    const ACharacter* TargetCharacter = Cast<ACharacter>(Target);
    UCapsuleComponent* TargetCapsule = TargetCharacter ? TargetCharacter->GetCapsuleComponent() : nullptr;
    if (!IsValid(Target) || !MonsterCapsule || !TargetCapsule)
    {
        return;
    }

    const FVector MonsterLocation = Monster.GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();
    FVector SeparationDirection = (MonsterLocation - TargetLocation).GetSafeNormal2D();
    if (SeparationDirection.IsNearlyZero())
    {
        SeparationDirection = -LockedChargeDirection;
    }

    const float RequiredDistance = MonsterCapsule->GetScaledCapsuleRadius()
        + TargetCapsule->GetScaledCapsuleRadius() + FMath::Max(0.0f, Config.EndOverlapSafetyPadding);
    const float CurrentDistance = FVector::Dist2D(MonsterLocation, TargetLocation);
    if (CurrentDistance >= RequiredDistance)
    {
        return;
    }

    const FVector CandidateLocation = MonsterLocation + SeparationDirection * (RequiredDistance - CurrentDistance);
    Monster.SetActorLocation(ProjectChargeLocationToGround(CandidateLocation, Config, Monster), false, nullptr,
        ETeleportType::TeleportPhysics);
    Monster.ForceNetUpdate();
}

FVector UASBossSpiderChargeAbility::ProjectChargeLocationToGround(const FVector& CandidateLocation,
    const FASBossSpiderChargeConfig& Config, const AASMonsterCharacter& Monster) const
{
    UWorld* World = Monster.GetWorld();
    if (!World)
    {
        return CandidateLocation;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossSpiderChargeGround), false, &Monster);
    QueryParams.AddIgnoredActor(&Monster);
    FHitResult GroundHit;
    const FVector TraceStart = CandidateLocation + FVector::UpVector * FMath::Max(0.0f, Config.GroundTraceStartHeight);
    const FVector TraceEnd = CandidateLocation - FVector::UpVector * FMath::Max(1.0f, Config.GroundTraceDepth);
    if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, Config.GroundTraceChannel, QueryParams))
    {
        return CandidateLocation;
    }

    const UCapsuleComponent* Capsule = Monster.GetCapsuleComponent();
    const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
    return FVector(CandidateLocation.X, CandidateLocation.Y,
        GroundHit.ImpactPoint.Z + CapsuleHalfHeight + FMath::Max(0.0f, Config.GroundClearance));
}

void UASBossSpiderChargeAbility::SpawnChargeIndicator(const FASBossSpiderChargeConfig& Config,
    AASMonsterCharacter& Monster)
{
    UWorld* World = Monster.GetWorld();
    if (!World || !Config.IndicatorClass)
    {
        return;
    }

    const float CapsuleRadius = Monster.GetCapsuleComponent()
        ? Monster.GetCapsuleComponent()->GetScaledCapsuleRadius()
        : 0.0f;
    const float LaneWidth = Config.IndicatorWidth > 0.0f
        ? Config.IndicatorWidth
        : (CapsuleRadius + FMath::Max(0.0f, Config.HitPadding)) * 2.0f;
    const FVector IndicatorLocation = ChargeStartLocation + LockedChargeDirection * (ResolvedTravelDistance * 0.5f);
    const FTransform SpawnTransform(LockedChargeDirection.Rotation(), IndicatorLocation);
    AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
        Config.IndicatorClass, SpawnTransform, &Monster, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Indicator)
    {
        return;
    }

    Indicator->InitializeRectangleIndicator(ResolvedTravelDistance, LaneWidth, Config.WindupTime);
    Indicator->FinishSpawning(SpawnTransform);
    ActiveIndicator = Indicator;
}

void UASBossSpiderChargeAbility::DestroyChargeIndicator()
{
    if (AASMonsterAttackIndicatorActor* Indicator = ActiveIndicator.Get())
    {
        Indicator->Destroy();
    }

    ActiveIndicator.Reset();
}

void UASBossSpiderChargeAbility::ApplyChargeDamage(const FASBossSpiderChargeConfig& Config,
    AASMonsterCharacter& Monster)
{
    UWorld* World = Monster.GetWorld();
    UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
    UCapsuleComponent* Capsule = Monster.GetCapsuleComponent();
    if (!CombatTargets || !Capsule)
    {
        return;
    }

    FASCombatTargetQuery Query;
    Query.SearchOrigin = Monster.GetActorLocation();
    Query.SearchRadius = Capsule->GetScaledCapsuleRadius() + FMath::Max(0.0f, Config.HitPadding);
    Query.Instigator = &Monster;
    Query.bCanHitPlayers = true;

    TArray<FGameplayEffectSpecHandle> EffectSpecs;
    if (FGameplayEffectSpecHandle DamageSpec = MakeConfiguredDamageSpec(Config); DamageSpec.IsValid())
    {
        EffectSpecs.Add(MoveTemp(DamageSpec));
    }
    if (FGameplayEffectSpecHandle StatusSpec = MakeConfiguredStatusSpec(Config); StatusSpec.IsValid())
    {
        EffectSpecs.Add(MoveTemp(StatusSpec));
    }
    if (EffectSpecs.IsEmpty())
    {
        return;
    }

    FASCombatImpactContext ImpactContext;
    ImpactContext.Preset = Config.ImpactPreset;
    ImpactContext.HitLocation = Monster.GetActorLocation();
    ImpactContext.HitDirection = LockedChargeDirection;
    CombatTargets->ResolveAndApply(Query, MakeArrayView(EffectSpecs), ImpactContext, &HitTargets);
}

FGameplayEffectSpecHandle UASBossSpiderChargeAbility::MakeConfiguredDamageSpec(const FASBossSpiderChargeConfig& Config) const
{
    if (!Config.DamageEffectClass)
    {
        return FGameplayEffectSpecHandle();
    }

    FGameplayEffectSpecHandle DamageSpec = MakeOutgoingGameplayEffectSpec(Config.DamageEffectClass, GetAbilityLevel());
    if (!DamageSpec.IsValid())
    {
        return DamageSpec;
    }

    float AttackPower = 0.0f;
    if (const IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(GetAvatarActorFromActorInfo()))
    {
        if (const UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemOwner->GetAbilitySystemComponent())
        {
            AttackPower = AbilitySystemComponent->GetNumericAttribute(UASCombatAttributeSet::GetAttackPowerAttribute());
        }
    }

    const float Damage = FMath::Max(0.0f, Config.BaseDamage + AttackPower * Config.AttackPowerScale);
    DamageSpec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -Damage);
    return DamageSpec;
}

FGameplayEffectSpecHandle UASBossSpiderChargeAbility::MakeConfiguredStatusSpec(const FASBossSpiderChargeConfig& Config) const
{
    return Config.OnHitStatusEffectClass
        ? MakeOutgoingGameplayEffectSpec(Config.OnHitStatusEffectClass, GetAbilityLevel())
        : FGameplayEffectSpecHandle();
}
