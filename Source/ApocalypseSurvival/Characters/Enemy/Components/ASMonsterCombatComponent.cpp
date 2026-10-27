#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"

#include "AbilitySystemInterface.h"
#include "Characters/Enemy/ASMonsterCharacter.h" // 추가됨: 체력 조회를 위한 몬스터 캐릭터 헤더
#include "GAS/Attributes/ASHealthAttributeSet.h" // 추가됨: 체력 AttributeSet 헤더
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GAS/ASAbilitySystemComponent.h"

namespace
{
    float GetCapsuleRadius(const AActor* Actor)
    {
       const ACharacter* Character = Cast<ACharacter>(Actor);
       return Character ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
    }
}

UASMonsterCombatComponent::UASMonsterCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UASMonsterCombatComponent::Configure(float InAttackRange, FGameplayTag InAttackAbilityTag)
{
    AttackRange = FMath::Max(0.0f, InAttackRange);
    AttackAbilityTag = InAttackAbilityTag;
}

void UASMonsterCombatComponent::ConfigurePatterns(const TArray<FASMonsterAttackPattern>& InPatterns,
    const TArray<FGameplayAbilitySpecHandle>& InAbilityHandles)
{
    AttackPatterns = InPatterns;
    PatternRuntime.Reset();
    PatternRuntime.Reserve(AttackPatterns.Num());
    NextAttackAllowedTime = 0.0;
    bWaitingForPatternAttackEnd = false;

    for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
    {
       FPatternRuntime& Runtime = PatternRuntime.AddDefaulted_GetRef();
       if (InAbilityHandles.IsValidIndex(PatternIndex))
       {
          Runtime.AbilityHandle = InAbilityHandles[PatternIndex];
       }
    }
}

// 새로 추가된 체력 비율 조회 함수 구현
float UASMonsterCombatComponent::GetHealthPercent() const
{
    const AActor* OwnerActor = GetOwner();
    if (!IsValid(OwnerActor))
    {
        return 1.0f;
    }

    if (const AASMonsterCharacter* MonsterChar = Cast<AASMonsterCharacter>(OwnerActor))
    {
        if (const UASAbilitySystemComponent* ASC = Cast<UASAbilitySystemComponent>(MonsterChar->GetAbilitySystemComponent()))
        {
            float CurrentHealth = ASC->GetNumericAttribute(UASHealthAttributeSet::GetHealthAttribute());
            float MaxHealth = ASC->GetNumericAttribute(UASHealthAttributeSet::GetMaxHealthAttribute());

            if (MaxHealth > 0.0f)
            {
                return FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f);
            }
        }
    }

    return 1.0f;
}

bool UASMonsterCombatComponent::IsTargetInAttackRange(const AActor* Target) const
{
    const AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !IsValid(Target))
    {
       return false;
    }

    const float CenterDistance = FVector::Dist2D(Owner->GetActorLocation(), Target->GetActorLocation());
    const float SurfaceDistance = CenterDistance - GetCapsuleRadius(Owner) - GetCapsuleRadius(Target);
    return SurfaceDistance <= AttackRange;
}

bool UASMonsterCombatComponent::CanRequestAttack(const AActor* Target) const
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Target))
    {
       return false;
    }

    if (!UsesAttackPatterns())
    {
       return IsTargetInAttackRange(Target);
    }

    if (bWaitingForPatternAttackEnd)
    {
       return false;
    }

    const double CurrentTime = GetWorldTimeSeconds();
    if (CurrentTime < NextAttackAllowedTime)
    {
       return false;
    }

    for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
    {
       if (IsPatternReady(PatternIndex, CurrentTime) && IsPatternInRange(AttackPatterns[PatternIndex], Target))
       {
          return true;
       }
    }

    return false;
}

bool UASMonsterCombatComponent::TryRequestAttack(AActor* Target)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Target))
    {
       return false;
    }

    if (UsesAttackPatterns())
    {
       return TryRequestPatternAttack(Target);
    }

    if (!AttackAbilityTag.IsValid() || !IsTargetInAttackRange(Target))
    {
       return false;
    }

    UASAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilitySystemComponent();
    if (!IsValid(AbilitySystemComponent)
       || AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Dead)
       || AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_HitReact))
    {
       return false;
    }

    FGameplayTagContainer AbilityTags;
    AbilityTags.AddTag(AttackAbilityTag);
    return AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags, false);
}

bool UASMonsterCombatComponent::IsPatternAttackActive() const
{
    if (!bWaitingForPatternAttackEnd)
    {
       return false;
    }

    const UASAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilitySystemComponent();
    return IsValid(AbilitySystemComponent)
       && AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Attacking);
}

void UASMonsterCombatComponent::NotifyPatternAttackFinished()
{
    if (!bWaitingForPatternAttackEnd)
    {
       return;
    }

    HideTelegraph();
    bWaitingForPatternAttackEnd = false;
    NextAttackAllowedTime = GetWorldTimeSeconds() + FMath::Max(0.0f, PostAttackRecovery);
}

bool UASMonsterCombatComponent::IsPatternInRange(const FASMonsterAttackPattern& Pattern, const AActor* Target) const
{
    const AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !IsValid(Target))
    {
       return false;
    }

    const float SurfaceDistance = FVector::Dist2D(Owner->GetActorLocation(), Target->GetActorLocation())
       - GetCapsuleRadius(Owner) - GetCapsuleRadius(Target);
    const float MinRange = FMath::Max(0.0f, Pattern.MinRange);
    const float MaxRange = FMath::Max(MinRange, Pattern.MaxRange);
    return SurfaceDistance >= MinRange && SurfaceDistance <= MaxRange;
}

bool UASMonsterCombatComponent::IsPatternReady(int32 PatternIndex, double CurrentTime) const
{
    if (!AttackPatterns.IsValidIndex(PatternIndex) || !PatternRuntime.IsValidIndex(PatternIndex))
    {
       return false;
    }

    const FASMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
    const FPatternRuntime& Runtime = PatternRuntime[PatternIndex];
    return Pattern.SelectionWeight > 0.0f && Runtime.AbilityHandle.IsValid() && CurrentTime >= Runtime.CooldownEndTime;
}

bool UASMonsterCombatComponent::TryRequestPatternAttack(AActor* Target)
{
    if (!CanRequestAttack(Target))
    {
       return false;
    }

    UASAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilitySystemComponent();
    if (!IsValid(AbilitySystemComponent)
       || AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Dead)
       || AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_HitReact))
    {
       return false;
    }

    const double CurrentTime = GetWorldTimeSeconds();
    TArray<int32> CandidateIndices;
    float TotalWeight = 0.0f;
    for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
    {
       if (IsPatternReady(PatternIndex, CurrentTime) && IsPatternInRange(AttackPatterns[PatternIndex], Target))
       {
          CandidateIndices.Add(PatternIndex);
          TotalWeight += AttackPatterns[PatternIndex].SelectionWeight;
       }
    }

    while (!CandidateIndices.IsEmpty() && TotalWeight > 0.0f)
    {
       float Roll = FMath::FRandRange(0.0f, TotalWeight);
       int32 SelectedCandidateArrayIndex = CandidateIndices.Num() - 1;
       for (int32 CandidateArrayIndex = 0; CandidateArrayIndex < CandidateIndices.Num(); ++CandidateArrayIndex)
       {
          const float Weight = AttackPatterns[CandidateIndices[CandidateArrayIndex]].SelectionWeight;
          if (Roll <= Weight)
          {
             SelectedCandidateArrayIndex = CandidateArrayIndex;
             break;
          }
          Roll -= Weight;
       }

       const int32 PatternIndex = CandidateIndices[SelectedCandidateArrayIndex];
       const FASMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
       const FPatternRuntime& Runtime = PatternRuntime[PatternIndex];
       if (AbilitySystemComponent->TryActivateAbility(Runtime.AbilityHandle))
       {
          PatternRuntime[PatternIndex].CooldownEndTime = CurrentTime + FMath::Max(0.0f, Pattern.Cooldown);
          bWaitingForPatternAttackEnd = true;
          return true;
       }

       TotalWeight -= Pattern.SelectionWeight;
       CandidateIndices.RemoveAtSwap(SelectedCandidateArrayIndex);
    }

    return false;
}

void UASMonsterCombatComponent::ShowTelegraphForPattern(int32 PatternIndex)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !AttackPatterns.IsValidIndex(PatternIndex))
    {
       return;
    }

    HideTelegraph();

    const FASMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
    if (Pattern.ShapeType == ETelegraphShape::None)
    {
       return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
       return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    FTransform SpawnTransform = Owner->GetTransform();
    CurrentTelegraphActor = World->SpawnActor<AASMonsterAttackIndicatorActor>(AASMonsterAttackIndicatorActor::StaticClass(), SpawnTransform, SpawnParams);

    if (CurrentTelegraphActor)
    {
       CurrentTelegraphActor->InitializeTelegraph(Pattern, Owner);
    }
}

void UASMonsterCombatComponent::HideTelegraph()
{
    if (IsValid(CurrentTelegraphActor))
    {
       CurrentTelegraphActor->Destroy();
       CurrentTelegraphActor = nullptr;
    }
}

void UASMonsterCombatComponent::ApplyPatternDamage(int32 PatternIndex)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !AttackPatterns.IsValidIndex(PatternIndex))
    {
       return;
    }

    const FASMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
    if (IsValid(CurrentTelegraphActor))
    {
       CurrentTelegraphActor->ApplyDamageAndExecute(Pattern, Owner);
    }

    HideTelegraph();
}

double UASMonsterCombatComponent::GetWorldTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? World->GetTimeSeconds() : 0.0;
}

UASAbilitySystemComponent* UASMonsterCombatComponent::GetOwnerAbilitySystemComponent() const
{
    if (const IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
       return Cast<UASAbilitySystemComponent>(AbilitySystemOwner->GetAbilitySystemComponent());
    }

    return nullptr;
}