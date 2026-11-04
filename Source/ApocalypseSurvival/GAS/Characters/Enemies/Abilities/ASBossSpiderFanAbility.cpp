#include "GAS/Characters/Enemies/Abilities/ASBossSpiderFanAbility.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Components/CapsuleComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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

void UASBossSpiderFanAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
    FASBossSpiderFanConfig ConfigSnapshot;
    FASMonsterPatternEntry PatternSnapshot;
    if (!IsValid(Monster) || !IsValid(Target)
        || !ResolveFanConfig(ConfigSnapshot, PatternSnapshot)
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
    LockedFacing = InitialFacing;
    bHasActiveConfig = true;
    LockedAttackOrigin = ResolveLockedAttackOrigin(*Monster);
    bCleanupComplete = false;
    StopAIMovement();
    UpdateFacing();

    if (ActivePattern.FacingPolicy != EASMonsterPatternFacingPolicy::None)
    {
        Monster->GetWorldTimerManager().SetTimer(FacingTimerHandle, this, &ThisClass::UpdateFacing,
            FMath::Max(0.005f, ActiveConfig.FacingUpdateInterval), true);
    }

    SpawnFanIndicator();
    OnFanWindupStarted();
    if (ActiveConfig.WindupTime <= 0.0f)
    {
        BeginFanStrike();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(WindupTimerHandle, this, &ThisClass::BeginFanStrike,
        ActiveConfig.WindupTime, false);
}

void UASBossSpiderFanAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const bool bReplicatedEndAbility, const bool bWasCancelled)
{
    CleanupFan(bWasCancelled);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

bool UASBossSpiderFanAbility::ResolveFanConfig(FASBossSpiderFanConfig& OutConfig,
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
        if (!Definition->PatternSet->ResolvePatternConfig(PatternId, Pattern))
        {
            continue;
        }

        if (const FASBossSpiderFanConfig* Config = Pattern.RuntimeConfig.GetPtr<FASBossSpiderFanConfig>())
        {
            OutConfig = *Config;
            OutPattern = Pattern;
            return true;
        }
    }

    return false;
}

bool UASBossSpiderFanAbility::IsTargetInActivationRange(const AASMonsterCharacter& Monster,
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

void UASBossSpiderFanAbility::BeginFanStrike()
{
    if (!IsActive() || !bHasActiveConfig)
    {
        CancelFan();
        return;
    }

    DestroyFanIndicator();
    UpdateFacing();
    ApplyFanDamage();
    OnFanStrike();
    BeginRecovery();
}

void UASBossSpiderFanAbility::BeginRecovery()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster) || !bHasActiveConfig)
    {
        CancelFan();
        return;
    }

    if (ActiveConfig.RecoveryTime <= 0.0f)
    {
        FinishFan();
        return;
    }

    Monster->GetWorldTimerManager().SetTimer(RecoveryTimerHandle, this, &ThisClass::FinishFan,
        ActiveConfig.RecoveryTime, false);
}

void UASBossSpiderFanAbility::FinishFan()
{
    if (IsActive())
    {
        FinishComboStep(false);
    }
}

void UASBossSpiderFanAbility::CancelFan()
{
    if (IsActive())
    {
        FinishComboStep(true);
    }
}

void UASBossSpiderFanAbility::CleanupFan(const bool bWasCancelled)
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

    DestroyFanIndicator();
    RestoreCharacterMovement();
    RestoreAIFacing();
    LockedTarget.Reset();
    LockedAttackOrigin = FVector::ZeroVector;
    bHasActiveConfig = false;
    OnFanFinished(bWasCancelled);
}

void UASBossSpiderFanAbility::UpdateFacing()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    if (!IsValid(Monster) || ActivePattern.FacingPolicy == EASMonsterPatternFacingPolicy::None)
    {
        return;
    }

    if (ActivePattern.FacingPolicy == EASMonsterPatternFacingPolicy::TrackTarget)
    {
        if (const AActor* Target = LockedTarget.Get())
        {
            const FVector TargetFacing = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
            if (!TargetFacing.IsNearlyZero())
            {
                LockedFacing = TargetFacing;
            }
        }
    }

    Monster->SetActorRotation(LockedFacing.Rotation());
}

FVector UASBossSpiderFanAbility::ResolveLockedAttackOrigin(const AASMonsterCharacter& Monster) const
{
    return Monster.GetActorLocation()
        + LockedFacing * ActiveConfig.AttackOriginForwardOffset;
}

void UASBossSpiderFanAbility::StopAIMovement()
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

void UASBossSpiderFanAbility::RestoreAIFacing()
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

void UASBossSpiderFanAbility::RestoreCharacterMovement()
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

void UASBossSpiderFanAbility::SpawnFanIndicator()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    if (!World || !ActiveConfig.IndicatorClass)
    {
        return;
    }

    const FTransform SpawnTransform(LockedFacing.Rotation(), LockedAttackOrigin);
    AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
        ActiveConfig.IndicatorClass, SpawnTransform, Monster, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Indicator)
    {
        return;
    }

    Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Sector, ActiveConfig.SectorRadius, 0.0f, 0.0f,
        ActiveConfig.WindupTime, ActiveConfig.SectorAngleDegrees);
    Indicator->FinishSpawning(SpawnTransform);
    ActiveIndicator = Indicator;
}

void UASBossSpiderFanAbility::DestroyFanIndicator()
{
    if (AASMonsterAttackIndicatorActor* Indicator = ActiveIndicator.Get())
    {
        Indicator->Destroy();
    }
    ActiveIndicator.Reset();
}

void UASBossSpiderFanAbility::ApplyFanDamage()
{
    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    UWorld* World = Monster ? Monster->GetWorld() : nullptr;
    UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
    if (!CombatTargets || !IsValid(Monster))
    {
        return;
    }

    TArray<FGameplayEffectSpecHandle> EffectSpecs;
    if (FGameplayEffectSpecHandle DamageSpec = MakeConfiguredDamageSpec(); DamageSpec.IsValid())
    {
        EffectSpecs.Add(MoveTemp(DamageSpec));
    }
    if (FGameplayEffectSpecHandle StatusSpec = MakeConfiguredStatusSpec(); StatusSpec.IsValid())
    {
        EffectSpecs.Add(MoveTemp(StatusSpec));
    }
    if (EffectSpecs.IsEmpty())
    {
        return;
    }

    FASCombatTargetQuery Query;
    Query.Shape = EASCombatTargetShape::Cone;
    Query.SearchOrigin = LockedAttackOrigin;
    Query.Direction = LockedFacing;
    Query.SearchRadius = FMath::Max(1.0f, ActiveConfig.SectorRadius);
    Query.AngleDegrees = FMath::Clamp(ActiveConfig.SectorAngleDegrees, 1.0f, 360.0f);
    Query.Instigator = Monster;
    Query.Domain = EASCombatTargetDomain::ActorOnly;
    Query.bCanHitPlayers = true;

    FASCombatImpactContext ImpactContext;
    ImpactContext.Preset = ActiveConfig.ImpactPreset;
    ImpactContext.HitLocation = LockedAttackOrigin;
    ImpactContext.HitDirection = LockedFacing;
    CombatTargets->ResolveAndApply(Query, MakeArrayView(EffectSpecs), ImpactContext);
}

FGameplayEffectSpecHandle UASBossSpiderFanAbility::MakeConfiguredDamageSpec() const
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

    const float Damage = FMath::Max(0.0f, ActiveConfig.BaseDamage + AttackPower * ActiveConfig.AttackPowerScale);
    DamageSpec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -Damage);
    return DamageSpec;
}

FGameplayEffectSpecHandle UASBossSpiderFanAbility::MakeConfiguredStatusSpec() const
{
    return ActiveConfig.OnHitStatusEffectClass
        ? MakeOutgoingGameplayEffectSpec(ActiveConfig.OnHitStatusEffectClass, GetAbilityLevel())
        : FGameplayEffectSpecHandle();
}
