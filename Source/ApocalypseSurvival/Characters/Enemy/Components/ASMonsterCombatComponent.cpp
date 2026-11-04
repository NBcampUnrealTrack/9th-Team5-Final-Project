#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"

#include "AbilitySystemInterface.h"
#include "Characters/Enemy/ASMonsterCharacter.h" // 추가됨: 체력 조회를 위한 몬스터 캐릭터 헤더
#include "GAS/Attributes/ASHealthAttributeSet.h" // 추가됨: 체력 AttributeSet 헤더
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Abilities/GameplayAbilityTypes.h"
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

void UASMonsterCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UASAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get(); ASC && AbilityEndedHandle.IsValid())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	}
	AbilityEndedHandle.Reset();
	BoundAbilitySystemComponent.Reset();
	Super::EndPlay(EndPlayReason);
}

void UASMonsterCombatComponent::Configure(float InAttackRange, FGameplayTag InAttackAbilityTag)
{
    AttackRange = FMath::Max(0.0f, InAttackRange);
    AttackAbilityTag = InAttackAbilityTag;
}

void UASMonsterCombatComponent::ConfigurePatterns(const TArray<FASMonsterPatternEntry>& InPatterns,
    const TArray<FGameplayAbilitySpecHandle>& InAbilityHandles,
    const EASMonsterPatternSelectionPolicy InSelectionPolicy)
{
    AttackPatterns = InPatterns;
    PatternSelectionPolicy = InSelectionPolicy;
    PatternRuntime.Reset();
    PatternRuntime.Reserve(AttackPatterns.Num());
    NextAttackAllowedTime = 0.0;
    bWaitingForPatternAttackEnd = false;
	ActivePatternAbilityHandle = FGameplayAbilitySpecHandle();
	LastStartedPatternId = FGameplayTag();
	PatternStartSerial = 0;
	EnsureAbilityEndedBinding();

    for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
    {
       FPatternRuntime& Runtime = PatternRuntime.AddDefaulted_GetRef();
       if (InAbilityHandles.IsValidIndex(PatternIndex))
       {
          Runtime.AbilityHandle = InAbilityHandles[PatternIndex];
       }
    }
}

void UASMonsterCombatComponent::ConfigureLegacyTelegraphPatterns(const TArray<FASMonsterAttackPattern>& InPatterns)
{
    LegacyTelegraphPatterns = InPatterns;
}

void UASMonsterCombatComponent::SetInterPatternDelay(const float InInterPatternDelay)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
       PostAttackRecovery = FMath::Max(0.0f, InInterPatternDelay);
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

bool UASMonsterCombatComponent::TryGetAutonomousPatternApproachRange(float& OutRange) const
{
	if (PatternSelectionPolicy == EASMonsterPatternSelectionPolicy::StateTreeDriven)
    {
        OutRange = 0.0f;
        return false;
    }

    float LargestEffectiveMaxRange = 0.0f;
    bool bHasAutonomousPattern = false;

    for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
    {
        if (!PatternRuntime.IsValidIndex(PatternIndex))
        {
            continue;
        }

        const FASMonsterPatternEntry& Pattern = AttackPatterns[PatternIndex];
        if (!Pattern.bEnabled || Pattern.SelectionWeight <= 0.0f
            || !PatternRuntime[PatternIndex].AbilityHandle.IsValid())
        {
            continue;
        }

        const float MinRange = FMath::Max(0.0f, Pattern.MinRange);
        float EffectiveMaxRange = FMath::Max(MinRange, Pattern.MaxRange);
        if (Pattern.MaxGuaranteedTargetSurfaceDistance > 0.0f)
        {
            EffectiveMaxRange = FMath::Min(EffectiveMaxRange, Pattern.MaxGuaranteedTargetSurfaceDistance);
        }

        LargestEffectiveMaxRange = FMath::Max(LargestEffectiveMaxRange, EffectiveMaxRange);
        bHasAutonomousPattern = true;
    }

    OutRange = LargestEffectiveMaxRange;
    return bHasAutonomousPattern;
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

	if (PatternSelectionPolicy == EASMonsterPatternSelectionPolicy::StateTreeDriven)
    {
       return false;
    }

    if (bWaitingForPatternAttackEnd)
    {
       return false;
    }

    const double CurrentTime = GetWorldTimeSeconds();
    if (CurrentTime < NextAttackAllowedTime)
    {
       for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
       {
          const FASMonsterPatternEntry& Pattern = AttackPatterns[PatternIndex];
          if (Pattern.bCanStartDuringInterPatternDelay
             && IsPatternReady(PatternIndex, CurrentTime, true)
             && IsPatternInRange(Pattern, Target))
          {
             return true;
          }
       }

       return false;
    }

    for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
    {
       if (IsPatternReady(PatternIndex, CurrentTime, true)
          && IsPatternInRange(AttackPatterns[PatternIndex], Target))
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

bool UASMonsterCombatComponent::CanRequestPatternById(FGameplayTag PatternId, const AActor* Target) const
{
    if (!PatternId.IsValid())
    {
       return false;
    }

    return CanRequestPatternAtIndex(FindPatternIndexById(PatternId), Target, false);
}

bool UASMonsterCombatComponent::CanPreparePatternById(const FGameplayTag PatternId, const AActor* Target) const
{
	if (!PatternId.IsValid())
	{
		return false;
	}

	return CanPreparePatternAtIndex(FindPatternIndexById(PatternId), Target, false);
}

bool UASMonsterCombatComponent::TryRequestPatternAttackById(FGameplayTag PatternId, AActor* Target)
{
    if (!PatternId.IsValid())
    {
       return false;
    }

    return TryRequestPatternAttackAtIndex(FindPatternIndexById(PatternId), Target, false);
}

bool UASMonsterCombatComponent::IsTargetInPatternRangeById(const FGameplayTag PatternId, const AActor* Target) const
{
	const int32 PatternIndex = FindPatternIndexById(PatternId);
	return AttackPatterns.IsValidIndex(PatternIndex)
		&& AttackPatterns[PatternIndex].bEnabled
		&& IsPatternInRange(AttackPatterns[PatternIndex], Target);
}

bool UASMonsterCombatComponent::TryGetPatternRangeById(const FGameplayTag PatternId,
	float& OutMinRange, float& OutMaxRange) const
{
	OutMinRange = 0.0f;
	OutMaxRange = 0.0f;
	const int32 PatternIndex = FindPatternIndexById(PatternId);
	if (!AttackPatterns.IsValidIndex(PatternIndex) || !AttackPatterns[PatternIndex].bEnabled)
	{
		return false;
	}

	const FASMonsterPatternEntry& Pattern = AttackPatterns[PatternIndex];
	OutMinRange = FMath::Max(0.0f, Pattern.MinRange);
	OutMaxRange = FMath::Max(OutMinRange, Pattern.MaxRange);
	if (Pattern.MaxGuaranteedTargetSurfaceDistance > 0.0f)
	{
		OutMaxRange = FMath::Min(OutMaxRange, Pattern.MaxGuaranteedTargetSurfaceDistance);
	}
	return OutMaxRange >= OutMinRange;
}

bool UASMonsterCombatComponent::IsPatternAttackActive() const
{
	if (!bWaitingForPatternAttackEnd || !ActivePatternAbilityHandle.IsValid())
    {
       return false;
    }

    const UASAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilitySystemComponent();
	const FGameplayAbilitySpec* ActiveSpec = IsValid(AbilitySystemComponent)
		? AbilitySystemComponent->FindAbilitySpecFromHandle(ActivePatternAbilityHandle)
		: nullptr;
	return ActiveSpec && ActiveSpec->IsActive();
}

void UASMonsterCombatComponent::NotifyPatternAttackFinished()
{
    if (!bWaitingForPatternAttackEnd)
    {
       return;
    }

	if (!IsPatternAttackActive())
	{
		FinishPatternAttackLifecycle();
	}
}

bool UASMonsterCombatComponent::IsPatternInRange(const FASMonsterPatternEntry& Pattern, const AActor* Target) const
{
    const AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !IsValid(Target))
    {
       return false;
    }

    const float SurfaceDistance = FVector::Dist2D(Owner->GetActorLocation(), Target->GetActorLocation())
       - GetCapsuleRadius(Owner) - GetCapsuleRadius(Target);
    const float MinRange = FMath::Max(0.0f, Pattern.MinRange);
    float MaxRange = FMath::Max(MinRange, Pattern.MaxRange);
    if (Pattern.MaxGuaranteedTargetSurfaceDistance > 0.0f)
    {
       MaxRange = FMath::Min(MaxRange, Pattern.MaxGuaranteedTargetSurfaceDistance);
    }

    return SurfaceDistance >= MinRange && SurfaceDistance <= MaxRange;
}

bool UASMonsterCombatComponent::IsPatternReady(int32 PatternIndex, double CurrentTime, const bool bRequireSelectionWeight) const
{
    if (!AttackPatterns.IsValidIndex(PatternIndex) || !PatternRuntime.IsValidIndex(PatternIndex))
    {
       return false;
    }

    const FASMonsterPatternEntry& Pattern = AttackPatterns[PatternIndex];
    const FPatternRuntime& Runtime = PatternRuntime[PatternIndex];
    const float MinimumHealthPercent = FMath::Min(Pattern.MinOwnerHealthPercent, Pattern.MaxOwnerHealthPercent);
    const float MaximumHealthPercent = FMath::Max(Pattern.MinOwnerHealthPercent, Pattern.MaxOwnerHealthPercent);
    const float OwnerHealthPercent = GetHealthPercent();
    return Pattern.bEnabled
       && (!bRequireSelectionWeight || Pattern.SelectionWeight > 0.0f)
       && Runtime.AbilityHandle.IsValid()
       && CurrentTime >= Runtime.CooldownEndTime
       && OwnerHealthPercent >= MinimumHealthPercent
       && OwnerHealthPercent <= MaximumHealthPercent;
}

int32 UASMonsterCombatComponent::FindPatternIndexById(const FGameplayTag PatternId) const
{
    return AttackPatterns.IndexOfByPredicate([PatternId](const FASMonsterPatternEntry& Pattern)
    {
       return Pattern.PatternId.MatchesTagExact(PatternId);
    });
}

bool UASMonsterCombatComponent::CanPreparePatternAtIndex(const int32 PatternIndex, const AActor* Target,
    const bool bRequireSelectionWeight) const
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Target)
       || !AttackPatterns.IsValidIndex(PatternIndex) || bWaitingForPatternAttackEnd)
    {
       return false;
    }

    const FASMonsterPatternEntry& Pattern = AttackPatterns[PatternIndex];
    const double CurrentTime = GetWorldTimeSeconds();
    return (CurrentTime >= NextAttackAllowedTime || Pattern.bCanStartDuringInterPatternDelay)
		&& IsPatternReady(PatternIndex, CurrentTime, bRequireSelectionWeight);
}

bool UASMonsterCombatComponent::CanRequestPatternAtIndex(const int32 PatternIndex, const AActor* Target,
	const bool bRequireSelectionWeight) const
{
	return CanPreparePatternAtIndex(PatternIndex, Target, bRequireSelectionWeight)
		&& IsPatternInRange(AttackPatterns[PatternIndex], Target);
}

bool UASMonsterCombatComponent::TryRequestPatternAttackAtIndex(const int32 PatternIndex, AActor* Target,
    const bool bRequireSelectionWeight)
{
    if (!CanRequestPatternAtIndex(PatternIndex, Target, bRequireSelectionWeight))
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
    FPatternRuntime& Runtime = PatternRuntime[PatternIndex];
	TGuardValue<TWeakObjectPtr<AActor>> RequestedTargetGuard(RequestedPatternTarget, Target);
	EnsureAbilityEndedBinding();
	ActivePatternAbilityHandle = Runtime.AbilityHandle;
	bWaitingForPatternAttackEnd = true;
    if (!AbilitySystemComponent->TryActivateAbility(Runtime.AbilityHandle))
    {
		if (ActivePatternAbilityHandle == Runtime.AbilityHandle)
		{
			ActivePatternAbilityHandle = FGameplayAbilitySpecHandle();
			bWaitingForPatternAttackEnd = false;
		}
       return false;
    }

    const FASMonsterPatternEntry& Pattern = AttackPatterns[PatternIndex];
    Runtime.CooldownEndTime = CurrentTime + FMath::Max(0.0f, Pattern.Cooldown);
	LastStartedPatternId = Pattern.PatternId;
	++PatternStartSerial;

	// 즉발 Ability가 TryActivateAbility 내부에서 이미 종료됐는데 delegate를 받지 못한 예외도 정리한다.
	if (bWaitingForPatternAttackEnd && !IsPatternAttackActive())
	{
		FinishPatternAttackLifecycle();
	}
    return true;
}

void UASMonsterCombatComponent::EnsureAbilityEndedBinding()
{
	UASAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
	if (!IsValid(ASC) || (BoundAbilitySystemComponent.Get() == ASC && AbilityEndedHandle.IsValid()))
	{
		return;
	}

	if (UASAbilitySystemComponent* PreviousASC = BoundAbilitySystemComponent.Get(); PreviousASC && AbilityEndedHandle.IsValid())
	{
		PreviousASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	}

	BoundAbilitySystemComponent = ASC;
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
}

void UASMonsterCombatComponent::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (bWaitingForPatternAttackEnd && EndedData.AbilitySpecHandle == ActivePatternAbilityHandle)
	{
		FinishPatternAttackLifecycle();
	}
}

void UASMonsterCombatComponent::FinishPatternAttackLifecycle()
{
	if (!bWaitingForPatternAttackEnd)
	{
		return;
	}

	HideTelegraph();
	bWaitingForPatternAttackEnd = false;
	ActivePatternAbilityHandle = FGameplayAbilitySpecHandle();
	NextAttackAllowedTime = GetWorldTimeSeconds() + FMath::Max(0.0f, PostAttackRecovery);
}

bool UASMonsterCombatComponent::TryRequestPatternAttack(AActor* Target)
{
    if (!CanRequestAttack(Target))
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
       if (TryRequestPatternAttackAtIndex(PatternIndex, Target, true))
       {
          return true;
       }

       TotalWeight -= AttackPatterns[PatternIndex].SelectionWeight;
       CandidateIndices.RemoveAtSwap(SelectedCandidateArrayIndex);
    }

    return false;
}

void UASMonsterCombatComponent::ShowTelegraphForPattern(int32 PatternIndex)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !LegacyTelegraphPatterns.IsValidIndex(PatternIndex))
    {
       return;
    }

    HideTelegraph();

    const FASMonsterAttackPattern& Pattern = LegacyTelegraphPatterns[PatternIndex];
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
    if (!Owner || !Owner->HasAuthority() || !LegacyTelegraphPatterns.IsValidIndex(PatternIndex))
    {
       return;
    }

    const FASMonsterAttackPattern& Pattern = LegacyTelegraphPatterns[PatternIndex];
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
