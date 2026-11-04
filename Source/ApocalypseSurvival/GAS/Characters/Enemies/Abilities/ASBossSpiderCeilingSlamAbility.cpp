#include "GAS/Characters/Enemies/Abilities/ASBossSpiderCeilingSlamAbility.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Skills/ASSkillTags.h"
#include "TimerManager.h"

namespace
{
    float GetActorCapsuleRadius(const AActor& Actor)
    {
        const ACharacter* Character = Cast<ACharacter>(&Actor);
        const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
        return Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f;
    }
}

void UASBossSpiderCeilingSlamAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
    FASBossSpiderCeilingSlamConfig ConfigSnapshot;
    FASMonsterPatternEntry PatternSnapshot;
    if (!IsValid(Monster) || !IsValid(Target)
        || !ResolveCeilingSlamConfig(ConfigSnapshot, PatternSnapshot)
        || !ConfigSnapshot.IndicatorClass || !ConfigSnapshot.DamageEffectClass || !ConfigSnapshot.CeilingUntargetableEffectClass
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
    LockedTarget = Target;
    RiseOrigin = Monster->GetActorLocation();
    LockedLandingLocation = ResolveGroundLocation(Target->GetActorLocation());
    LockedFacing = InitialFacing;
    NextShockwaveIndex = 0;
    bHasActiveConfig = true;
    bCleanupComplete = false;
    bIsElevated = false;
    bHasLanded = false;
    bMeshShadowStateCaptured = false;
    bPreviousMeshCastShadow = true;
    StopAIMovement();
    ApplyCeilingUntargetableEffect();
    BeginRise();
}

void UASBossSpiderCeilingSlamAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const bool bReplicatedEndAbility, const bool bWasCancelled)
{
    CleanupCeilingSlam(bWasCancelled);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

bool UASBossSpiderCeilingSlamAbility::ResolveCeilingSlamConfig(FASBossSpiderCeilingSlamConfig& OutConfig,
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
            if (const FASBossSpiderCeilingSlamConfig* Config = Pattern.RuntimeConfig.GetPtr<FASBossSpiderCeilingSlamConfig>())
            {
                OutConfig = *Config;
                OutPattern = Pattern;
                return true;
            }
        }
    }
    return false;
}

bool UASBossSpiderCeilingSlamAbility::IsTargetInActivationRange(const AASMonsterCharacter& Monster,
    const AActor& Target, const FASMonsterPatternEntry& Pattern) const
{
    const float SurfaceDistance = FVector::Dist2D(Monster.GetActorLocation(), Target.GetActorLocation())
        - GetActorCapsuleRadius(Monster) - GetActorCapsuleRadius(Target);
    const float MinRange = FMath::Max(0.0f, Pattern.MinRange);
    float MaxRange = FMath::Max(MinRange, Pattern.MaxRange);
    if (Pattern.MaxGuaranteedTargetSurfaceDistance > 0.0f)
    {
        MaxRange = FMath::Min(MaxRange, Pattern.MaxGuaranteedTargetSurfaceDistance);
    }
    return SurfaceDistance >= MinRange && SurfaceDistance <= MaxRange;
}

void UASBossSpiderCeilingSlamAbility::BeginRise()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster))
    {
        CancelCeilingSlam();
        return;
    }

    PhaseStartTime = Monster->GetWorld()->GetTimeSeconds();
    DisableElevatedMeshShadow();
    OnCeilingSlamRiseStarted();
    UpdateRise();
    Monster->GetWorldTimerManager().SetTimer(RiseTimerHandle, this, &ThisClass::UpdateRise, 0.02f, true);
}

void UASBossSpiderCeilingSlamAbility::UpdateRise()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsActive() || !bHasActiveConfig || !IsValid(Monster))
    {
        CancelCeilingSlam();
        return;
    }

    const float Alpha = FMath::Clamp(static_cast<float>((Monster->GetWorld()->GetTimeSeconds() - PhaseStartTime)
        / FMath::Max(0.05f, ActiveConfig.RiseDuration)), 0.0f, 1.0f);
    Monster->SetActorLocation(RiseOrigin + FVector::UpVector * ActiveConfig.RiseHeight * Alpha, false, nullptr,
        ETeleportType::TeleportPhysics);
    bIsElevated = true;
    if (Alpha >= 1.0f)
    {
        Monster->GetWorldTimerManager().ClearTimer(RiseTimerHandle);
        BeginTracking();
    }
}

void UASBossSpiderCeilingSlamAbility::BeginTracking()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster))
    {
        CancelCeilingSlam();
        return;
    }

    PhaseStartTime = Monster->GetWorld()->GetTimeSeconds();
    SpawnIndicator(FMath::Max(0.05f, ActiveConfig.TargetTrackingDuration), false);
    OnCeilingSlamTrackingStarted();
    UpdateTracking();
    if (ActiveConfig.TargetTrackingDuration > 0.0f)
    {
        Monster->GetWorldTimerManager().SetTimer(TrackingTimerHandle, this, &ThisClass::UpdateTracking, 0.02f, true);
    }
}

void UASBossSpiderCeilingSlamAbility::UpdateTracking()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsActive() || !bHasActiveConfig || !IsValid(Monster))
    {
        CancelCeilingSlam();
        return;
    }

    if (const AActor* Target = LockedTarget.Get())
    {
        LockedLandingLocation = ResolveGroundLocation(Target->GetActorLocation());
        const FVector UpdatedFacing = (LockedLandingLocation - RiseOrigin).GetSafeNormal2D();
        if (!UpdatedFacing.IsNearlyZero())
        {
            LockedFacing = UpdatedFacing;
        }
    }
    if (AASMonsterAttackIndicatorActor* Indicator = ActiveIndicator.Get())
    {
        Indicator->SetActorLocation(LockedLandingLocation, false, nullptr, ETeleportType::TeleportPhysics);
    }

    const float Elapsed = static_cast<float>(Monster->GetWorld()->GetTimeSeconds() - PhaseStartTime);
    if (Elapsed >= ActiveConfig.TargetTrackingDuration)
    {
        Monster->GetWorldTimerManager().ClearTimer(TrackingTimerHandle);
        BeginLandingTelegraph();
    }
}

void UASBossSpiderCeilingSlamAbility::BeginLandingTelegraph()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster))
    {
        CancelCeilingSlam();
        return;
    }

    DestroyIndicator();
    SpawnIndicator(ActiveConfig.LandingTelegraphDuration, true);
    OnCeilingSlamLandingTelegraph(LockedLandingLocation);
    Monster->GetWorldTimerManager().SetTimer(LandingTimerHandle, this, &ThisClass::LandAndApplyImpact,
        ActiveConfig.LandingTelegraphDuration, false);
}

void UASBossSpiderCeilingSlamAbility::LandAndApplyImpact()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsActive() || !bHasActiveConfig || !IsValid(Monster))
    {
        CancelCeilingSlam();
        return;
    }

    DestroyIndicator();
    // Indicator/damage/shockwave는 LockedLandingLocation을 고정 중심으로 유지한다. Capsule만 겹친
    // 플레이어 바깥으로 분리해 기존 BP OverlapGuard가 Gameplay 이동을 소유하지 않도록 한다.
    const FVector PhysicalLandingLocation = ResolvePhysicalLandingLocation(LockedLandingLocation);
    Monster->SetActorLocation(PhysicalLandingLocation, false, nullptr, ETeleportType::TeleportPhysics);
    bIsElevated = false;
    bHasLanded = true;
    RestoreElevatedMeshShadow();
    RemoveCeilingUntargetableEffect();
    OnCeilingSlamLanded(LockedLandingLocation);
    ApplyRadialDamage(ActiveConfig.LandingRadius, 1.0f, true);
    BeginNextShockwave();
}

void UASBossSpiderCeilingSlamAbility::BeginNextShockwave()
{
    if (!IsActive() || !bHasActiveConfig)
    {
        CancelCeilingSlam();
        return;
    }

    if (NextShockwaveIndex >= FMath::Clamp(ActiveConfig.ShockwaveCount, 0, 3))
    {
        BeginRecovery();
        return;
    }

    ++NextShockwaveIndex;
    ApplyRadialDamage(ActiveConfig.ShockwaveRadius, ActiveConfig.ShockwaveDamageMultiplier, false);
    OnCeilingSlamShockwave(NextShockwaveIndex, LockedLandingLocation, ActiveConfig.ShockwaveRadius);
    if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter()))
    {
        Monster->GetWorldTimerManager().SetTimer(ShockwaveTimerHandle, this, &ThisClass::BeginNextShockwave,
            FMath::Max(0.01f, ActiveConfig.ShockwaveInterval), false);
    }
}

void UASBossSpiderCeilingSlamAbility::BeginRecovery()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster) || !bHasActiveConfig)
    {
        CancelCeilingSlam();
        return;
    }

    if (ActiveConfig.RecoveryTime <= 0.0f)
    {
        FinishCeilingSlam();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(RecoveryTimerHandle, this, &ThisClass::FinishCeilingSlam,
        ActiveConfig.RecoveryTime, false);
}

void UASBossSpiderCeilingSlamAbility::FinishCeilingSlam()
{
    if (IsActive())
    {
        FinishComboStep(false);
    }
}

void UASBossSpiderCeilingSlamAbility::CancelCeilingSlam()
{
    if (IsActive())
    {
        FinishComboStep(true);
    }
}

void UASBossSpiderCeilingSlamAbility::CleanupCeilingSlam(const bool bWasCancelled)
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
            World->GetTimerManager().ClearTimer(RiseTimerHandle);
            World->GetTimerManager().ClearTimer(TrackingTimerHandle);
            World->GetTimerManager().ClearTimer(LandingTimerHandle);
            World->GetTimerManager().ClearTimer(ShockwaveTimerHandle);
            World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
        }
        if (bWasCancelled && bIsElevated && !bHasLanded)
        {
            Monster->SetActorLocation(RiseOrigin, false, nullptr, ETeleportType::TeleportPhysics);
        }
    }

    DestroyIndicator();
    RemoveCeilingUntargetableEffect();
    RestoreElevatedMeshShadow();
    RestoreCharacterMovement();
    RestoreAIFacing();
    LockedTarget.Reset();
    bHasActiveConfig = false;
    bIsElevated = false;
    OnCeilingSlamFinished(bWasCancelled);
}

void UASBossSpiderCeilingSlamAbility::DisableElevatedMeshShadow()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    USkeletalMeshComponent* MeshComponent = Monster ? Monster->GetMesh() : nullptr;
    if (!Monster || !MeshComponent || bMeshShadowStateCaptured)
    {
        return;
    }

    bPreviousMeshCastShadow = MeshComponent->CastShadow;
    bMeshShadowStateCaptured = true;
    Monster->SetMonsterMeshShadowEnabled(false);
}

void UASBossSpiderCeilingSlamAbility::RestoreElevatedMeshShadow()
{
    if (!bMeshShadowStateCaptured)
    {
        return;
    }

    if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter()))
    {
        Monster->SetMonsterMeshShadowEnabled(bPreviousMeshCastShadow);
    }
    bMeshShadowStateCaptured = false;
}

FVector UASBossSpiderCeilingSlamAbility::ResolveGroundLocation(const FVector& CandidateLocation) const
{
    const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    if (!World)
    {
        return CandidateLocation;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossSpiderCeilingLanding), false, Monster);
    // Landing/indicator 중심은 Pawn Capsule 표면이 아니라 실제 월드 바닥이어야 한다.
    for (TActorIterator<APawn> It(World); It; ++It)
    {
        QueryParams.AddIgnoredActor(*It);
    }
    FHitResult FloorHit;
    if (World->LineTraceSingleByChannel(FloorHit, CandidateLocation + FVector::UpVector * 1200.0f,
        CandidateLocation - FVector::UpVector * 2400.0f, ECC_WorldStatic, QueryParams)
        && FloorHit.ImpactNormal.Z >= 0.5f)
    {
        return FloorHit.ImpactPoint;
    }
    return CandidateLocation;
}

FVector UASBossSpiderCeilingSlamAbility::ResolvePhysicalLandingLocation(const FVector& ImpactLocation) const
{
    const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    const UCapsuleComponent* MonsterCapsule = Monster ? Monster->GetCapsuleComponent() : nullptr;
    if (!World || !MonsterCapsule)
    {
        return ImpactLocation;
    }

    const float MonsterRadius = MonsterCapsule->GetScaledCapsuleRadius();
    FVector ResolvedLocation = ImpactLocation;
    // 두 번 순회하면 2~4인 협동 플레이에서 첫 분리로 다른 Capsule과 겹치는 경우도 해소한다.
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        bool bAdjustedThisPass = false;
        for (TActorIterator<AASPlayerCharacter> It(World); It; ++It)
        {
            const AASPlayerCharacter* Player = *It;
            if (!IsValid(Player) || Player->IsDead() || Player->IsDowned())
            {
                continue;
            }

            const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();
            if (!PlayerCapsule)
            {
                continue;
            }

            FVector AwayFromPlayer = ResolvedLocation - Player->GetActorLocation();
            AwayFromPlayer.Z = 0.0f;
            const float CurrentDistance = AwayFromPlayer.Size();
            const float RequiredDistance = MonsterRadius + PlayerCapsule->GetScaledCapsuleRadius()
                + FMath::Max(0.0f, ActiveConfig.LandingCapsuleClearance);
            if (CurrentDistance >= RequiredDistance)
            {
                continue;
            }

            FVector SeparationDirection = CurrentDistance > KINDA_SMALL_NUMBER
                ? AwayFromPlayer / CurrentDistance
                : -LockedFacing.GetSafeNormal2D();
            if (SeparationDirection.IsNearlyZero())
            {
                SeparationDirection = FVector::BackwardVector;
            }
            ResolvedLocation += SeparationDirection * (RequiredDistance - CurrentDistance);
            bAdjustedThisPass = true;
        }
        if (!bAdjustedThisPass)
        {
            break;
        }
    }
    // LockedLandingLocation은 바닥 Impact Point다. Character Root는 Capsule 중심이므로 실제 착지에만 Half Height를 더한다.
    ResolvedLocation.Z = ImpactLocation.Z + MonsterCapsule->GetScaledCapsuleHalfHeight();
    return ResolvedLocation;
}

void UASBossSpiderCeilingSlamAbility::SpawnIndicator(const float Duration, const bool bUseVisualProgress)
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    if (!World || !ActiveConfig.IndicatorClass)
    {
        return;
    }

    const FTransform SpawnTransform(FRotator::ZeroRotator, LockedLandingLocation);
    AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
        ActiveConfig.IndicatorClass, SpawnTransform, Monster, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Indicator)
    {
        return;
    }

    // AASMonsterAttackIndicatorActor derives visual Fill from Duration. During target tracking the ring must only
    // move, so retain a near-zero progress until this Ability destroys it at location lock.
    const float IndicatorDuration = bUseVisualProgress ? FMath::Max(0.05f, Duration) : 3600.0f;
    Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Circle, ActiveConfig.LandingRadius, 0.0f, 0.0f,
        IndicatorDuration);
    Indicator->FinishSpawning(SpawnTransform);
    ActiveIndicator = Indicator;
}

void UASBossSpiderCeilingSlamAbility::DestroyIndicator()
{
    if (AASMonsterAttackIndicatorActor* Indicator = ActiveIndicator.Get())
    {
        Indicator->Destroy();
    }
    ActiveIndicator.Reset();
}

void UASBossSpiderCeilingSlamAbility::ApplyRadialDamage(const float Radius, const float DamageMultiplier,
    const bool bApplyStatus)
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
    if (!CombatTargets || !IsValid(Monster))
    {
        return;
    }

    TArray<FGameplayEffectSpecHandle> EffectSpecs;
    if (FGameplayEffectSpecHandle DamageSpec = MakeConfiguredDamageSpec(DamageMultiplier); DamageSpec.IsValid())
    {
        EffectSpecs.Add(MoveTemp(DamageSpec));
    }
    if (bApplyStatus)
    {
        if (FGameplayEffectSpecHandle StatusSpec = MakeConfiguredLandingStatusSpec(); StatusSpec.IsValid())
        {
            EffectSpecs.Add(MoveTemp(StatusSpec));
        }
    }
    if (EffectSpecs.IsEmpty())
    {
        return;
    }

    FASCombatTargetQuery Query;
    Query.Shape = EASCombatTargetShape::Radius;
    Query.SearchOrigin = LockedLandingLocation;
    Query.SearchRadius = FMath::Max(1.0f, Radius);
    Query.Instigator = Monster;
    Query.Domain = EASCombatTargetDomain::ActorOnly;
    Query.bCanHitPlayers = true;

    FASCombatImpactContext ImpactContext;
    ImpactContext.Preset = ActiveConfig.ImpactPreset;
    ImpactContext.HitLocation = LockedLandingLocation;
    ImpactContext.HitDirection = FVector::DownVector;
    CombatTargets->ResolveAndApply(Query, MakeArrayView(EffectSpecs), ImpactContext);
}

FGameplayEffectSpecHandle UASBossSpiderCeilingSlamAbility::MakeConfiguredDamageSpec(const float DamageMultiplier) const
{
    if (!ActiveConfig.DamageEffectClass)
    {
        return FGameplayEffectSpecHandle();
    }

    FGameplayEffectSpecHandle DamageSpec = MakeOutgoingGameplayEffectSpec(ActiveConfig.DamageEffectClass, GetAbilityLevel());
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
    const float Damage = FMath::Max(0.0f, (ActiveConfig.LandingBaseDamage
        + AttackPower * ActiveConfig.LandingAttackPowerScale) * FMath::Max(0.0f, DamageMultiplier));
    DamageSpec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -Damage);
    return DamageSpec;
}

FGameplayEffectSpecHandle UASBossSpiderCeilingSlamAbility::MakeConfiguredLandingStatusSpec() const
{
    return ActiveConfig.OnLandingStatusEffectClass
        ? MakeOutgoingGameplayEffectSpec(ActiveConfig.OnLandingStatusEffectClass, GetAbilityLevel())
        : FGameplayEffectSpecHandle();
}

void UASBossSpiderCeilingSlamAbility::ApplyCeilingUntargetableEffect()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UAbilitySystemComponent* ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
    if (!ASC || !ActiveConfig.CeilingUntargetableEffectClass)
    {
        return;
    }

    FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
    Context.AddSourceObject(this);
    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ActiveConfig.CeilingUntargetableEffectClass, GetAbilityLevel(), Context);
    if (Spec.IsValid())
    {
        CeilingUntargetableEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    }
}

void UASBossSpiderCeilingSlamAbility::RemoveCeilingUntargetableEffect()
{
    if (!CeilingUntargetableEffectHandle.IsValid())
    {
        return;
    }
    if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter()))
    {
        if (UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent())
        {
            ASC->RemoveActiveGameplayEffect(CeilingUntargetableEffectHandle);
        }
    }
    CeilingUntargetableEffectHandle.Invalidate();
}

void UASBossSpiderCeilingSlamAbility::StopAIMovement()
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

void UASBossSpiderCeilingSlamAbility::RestoreAIFacing()
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

void UASBossSpiderCeilingSlamAbility::RestoreCharacterMovement()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UCharacterMovementComponent* Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
    if (Movement && bMovementModeChanged)
    {
        Movement->SetMovementMode(PreviousMovementMode, PreviousCustomMovementMode);
        bMovementModeChanged = false;
    }
}
