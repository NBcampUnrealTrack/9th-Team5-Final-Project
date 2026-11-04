#include "Characters/Enemy/Components/ASBossSpiderDirectorComponent.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASMonsterPatternSet.h"
#include "Engine/World.h"

UASBossSpiderDirectorComponent::UASBossSpiderDirectorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UASBossSpiderDirectorComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
       SetComponentTickEnabled(false);
       return;
    }

    FASResolvedMonsterPatternSet ResolvedSet;
    const UASMonsterPatternSet*  MonsterPatternSet = GetPatternSet();
    const FASBossSpiderDirectorConfig* DirectorConfig = MonsterPatternSet && MonsterPatternSet->ResolvePatternSet(ResolvedSet)
        ? ResolvedSet.SelectionPolicyConfig.GetPtr<FASBossSpiderDirectorConfig>()
        : nullptr;
    if (!DirectorConfig)
    {
       UE_LOG(LogTemp, Error, TEXT("[BossSpider] %s requires FASBossSpiderDirectorConfig in MonsterDefinition->PatternSet."),
          *GetNameSafe(GetOwner()));
       SetComponentTickEnabled(false);
       return;
    }

    ActiveConfig = *DirectorConfig;
    bHasActiveConfig = true;
    const double CurrentTime = GetWorldTimeSeconds();
    NextDecisionTime = CurrentTime + FMath::Max(0.0f, ActiveConfig.DecisionInterval);
    NextMajorPatternTime = CurrentTime + FMath::Max(0.0f, ActiveConfig.MajorPatternInterval);
    NextWebTrapTime = CurrentTime + FMath::Max(0.0f, ActiveConfig.WebTrapInterval);
}

void UASBossSpiderDirectorComponent::SetCaptureTarget(AActor* NewCaptureTarget)
{
    TrySetCaptureTarget(NewCaptureTarget);
}

bool UASBossSpiderDirectorComponent::TrySetCaptureTarget(AActor* NewCaptureTarget)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
       return false;
    }

    if (!IsValid(NewCaptureTarget))
    {
       return false;
    }

    if (CaptureTarget.IsValid() && CaptureTarget.Get() != NewCaptureTarget)
    {
       return false;
    }

    CaptureTarget = NewCaptureTarget;
    return true;
}

void UASBossSpiderDirectorComponent::ClearCaptureTarget(AActor* ExpectedTarget)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
       || (IsValid(ExpectedTarget) && CaptureTarget.Get() != ExpectedTarget))
    {
       return;
    }

    CaptureTarget.Reset();
}

void UASBossSpiderDirectorComponent::SetEggsActive(const bool bInEggsActive)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
       bLegacyEggsActive = bInEggsActive;
    }
}

bool UASBossSpiderDirectorComponent::HasActiveEggs() const
{
    if (bLegacyEggsActive)
    {
        return true;
    }

    for (const TWeakObjectPtr<AActor>& EggActor : ActiveEggActors)
    {
        if (EggActor.IsValid())
        {
            return true;
        }
    }
    return false;
}

void UASBossSpiderDirectorComponent::RegisterActiveEgg(AActor* EggActor)
{
    if (GetOwner() && GetOwner()->HasAuthority() && IsValid(EggActor))
    {
        ActiveEggActors.Add(EggActor);
    }
}

void UASBossSpiderDirectorComponent::UnregisterActiveEgg(AActor* EggActor)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    if (IsValid(EggActor))
    {
        ActiveEggActors.Remove(TWeakObjectPtr<AActor>(EggActor));
    }
    for (auto It = ActiveEggActors.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }
}

bool UASBossSpiderDirectorComponent::TryRequestSpecialPattern(FGameplayTag PatternId, AActor* Target)
{
    return TryStartDirectorPattern(PatternId, Target);
}

void UASBossSpiderDirectorComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AASMonsterCharacter* BossMonster = GetBossMonster();
    if (!bHasActiveConfig || !IsValid(BossMonster) || BossMonster->IsDead())
    {
       SetComponentTickEnabled(false);
       return;
    }

    FinishDirectorPatternIfNeeded();
    if (bWaitingForDirectorPattern)
    {
       return;
    }

    const double CurrentTime = GetWorldTimeSeconds();
    if (CurrentTime < NextDecisionTime)
    {
       return;
    }
    NextDecisionTime = CurrentTime + FMath::Max(0.05f, ActiveConfig.DecisionInterval);

    if (CaptureTarget.IsValid())
    {
       if (TryStartDirectorPattern(ActiveConfig.CaptureChargePatternId, CaptureTarget.Get()))
       {
          CaptureTarget.Reset();
       }
       return;
    }

    UASMonsterTargetingComponent* TargetingComponent = BossMonster->GetTargetingComponent();
    AActor* Target = IsValid(TargetingComponent) ? TargetingComponent->GetTargetActor() : nullptr;
    if (!IsValid(Target))
    {
       return;
    }

    UASMonsterCombatComponent* CombatComponent = GetCombatComponent();
    if (!IsValid(CombatComponent))
    {
       return;
    }

    if (CombatComponent->GetHealthPercent() <= ActiveConfig.PhaseTwoHealthPercent && CurrentTime >= NextMajorPatternTime)
    {
       const bool bHasActiveEggs = HasActiveEggs();
       const FGameplayTag PreferredPattern = (!bHasActiveEggs && bNextMajorPatternPrefersEggs)
          ? ActiveConfig.EggSummonPatternId
          : ActiveConfig.CeilingSlamPatternId;
       const FGameplayTag FallbackPattern = PreferredPattern.MatchesTagExact(ActiveConfig.EggSummonPatternId)
          ? ActiveConfig.CeilingSlamPatternId
           : (!bHasActiveEggs ? ActiveConfig.EggSummonPatternId : FGameplayTag());

       if (TryStartDirectorPattern(PreferredPattern, Target))
       {
          bNextMajorPatternPrefersEggs = !PreferredPattern.MatchesTagExact(ActiveConfig.EggSummonPatternId);
          NextMajorPatternTime = CurrentTime + FMath::Max(0.0f, ActiveConfig.MajorPatternInterval);
          return;
       }

       if (FallbackPattern.IsValid() && TryStartDirectorPattern(FallbackPattern, Target))
       {
          bNextMajorPatternPrefersEggs = !FallbackPattern.MatchesTagExact(ActiveConfig.EggSummonPatternId);
          NextMajorPatternTime = CurrentTime + FMath::Max(0.0f, ActiveConfig.MajorPatternInterval);
          return;
       }
    }

    if (CurrentTime >= NextWebTrapTime && TryStartDirectorPattern(ActiveConfig.WebTrapPatternId, Target))
    {
       NextWebTrapTime = CurrentTime + FMath::Max(0.0f, ActiveConfig.WebTrapInterval);
    }
}

AASMonsterCharacter* UASBossSpiderDirectorComponent::GetBossMonster() const
{
    return Cast<AASMonsterCharacter>(GetOwner());
}

UASMonsterCombatComponent* UASBossSpiderDirectorComponent::GetCombatComponent() const
{
    if (AASMonsterCharacter* BossMonster = GetBossMonster())
    {
       return BossMonster->GetCombatComponent();
    }

    return nullptr;
}

const UASMonsterPatternSet* UASBossSpiderDirectorComponent::GetPatternSet() const
{
    const AASMonsterCharacter* BossMonster = GetBossMonster();
    const UASMonsterDefinition* Definition = BossMonster ? BossMonster->GetMonsterDefinition() : nullptr;
    return Definition ? Definition->PatternSet : nullptr;
}

bool UASBossSpiderDirectorComponent::TryStartDirectorPattern(const FGameplayTag PatternId, AActor* Target)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !PatternId.IsValid() || !IsValid(Target))
    {
       return false;
    }

    UASMonsterCombatComponent* CombatComponent = GetCombatComponent();
    if (!IsValid(CombatComponent) || !CombatComponent->TryRequestPatternAttackById(PatternId, Target))
    {
       return false;
    }

    bWaitingForDirectorPattern = true;
    return true;
}

void UASBossSpiderDirectorComponent::FinishDirectorPatternIfNeeded()
{
    if (!bWaitingForDirectorPattern)
    {
       return;
    }

    UASMonsterCombatComponent* CombatComponent = GetCombatComponent();
    if (!IsValid(CombatComponent) || !CombatComponent->IsPatternAttackActive())
    {
       if (IsValid(CombatComponent))
       {
          CombatComponent->NotifyPatternAttackFinished();
       }
       bWaitingForDirectorPattern = false;
    }
}

double UASBossSpiderDirectorComponent::GetWorldTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? World->GetTimeSeconds() : 0.0;
}
