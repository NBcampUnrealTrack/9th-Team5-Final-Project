#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Mass/Monster/Targeting/ASMonsterTargetable.h"
#include "Net/UnrealNetwork.h"

UASMonsterTargetingComponent::UASMonsterTargetingComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UASMonsterTargetingComponent::Configure(float InDetectionRadius, float InLoseTargetRadius, float InAggroDecayPerSecond)
{
    DetectionRadius = FMath::Max(0.0f, InDetectionRadius);
    LoseTargetRadius = FMath::Max(DetectionRadius, InLoseTargetRadius);
    AggroDecayPerSecond = FMath::Max(0.0f, InAggroDecayPerSecond);
}

APawn* UASMonsterTargetingComponent::FindNearestLivingPlayer(float MaxDistance) const
{
    const AActor* Owner = GetOwner();
    const UWorld* World = GetWorld();
    if (!IsValid(Owner) || !Owner->HasAuthority() || !World)
    {
       return nullptr;
    }

    APawn* NearestPawn = nullptr;
    float NearestDistance = MaxDistance;

    // 플레이어 컨트롤러는 서버에만 전부 있다.
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
       APawn* Pawn = It->IsValid() ? It->Get()->GetPawn() : nullptr;
       if (!IsLivingPlayerTarget(Pawn))
       {
          continue;
       }

       const float Distance = FVector::Dist2D(Owner->GetActorLocation(), Pawn->GetActorLocation());
       if (Distance <= NearestDistance)
       {
          NearestDistance = Distance;
          NearestPawn = Pawn;
       }
    }

    return NearestPawn;
}

bool UASMonsterTargetingComponent::AcquireTarget()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
       return false;
    }

    // 이미 쫓는 대상이 있으면 더 가까운 플레이어가 나타나도 바꾸지 않는다. 타깃이 계속 흔들리지 않게 한다.
    if (HasValidTarget() && IsLivingPlayerTarget(Cast<APawn>(TargetActor)))
    {
       return true;
    }

    APawn* NewTarget = FindNearestLivingPlayer(DetectionRadius);
    CurrentAggro = NewTarget ? 1.0f : 0.0f;
    SetTargetActor(NewTarget);
    return NewTarget != nullptr;
}

bool UASMonsterTargetingComponent::AcquireTargetAnywhere()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
       return false;
    }

    if (HasValidTarget() && IsLivingPlayerTarget(Cast<APawn>(TargetActor)))
    {
       return true;
    }

    APawn* NewTarget = FindNearestLivingPlayer(TNumericLimits<float>::Max());
    CurrentAggro = NewTarget ? 1.0f : 0.0f;
    SetTargetActor(NewTarget);
    return NewTarget != nullptr;
}

bool UASMonsterTargetingComponent::ValidateTarget(bool bIgnoreLoseTargetRadius)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
       return false;
    }

    const APawn* TargetPawn = Cast<APawn>(TargetActor);
    const bool bStillValid = HasValidTarget() && IsLivingPlayerTarget(TargetPawn)
       && (bIgnoreLoseTargetRadius || CurrentAggro > 0.0f);

    if (!bStillValid)
    {
       ClearTarget();
    }

    return bStillValid;
}

bool UASMonsterTargetingComponent::ForceAggro(AActor* Aggressor, float AggroAmount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || AggroAmount <= 0.0f)
    {
       return false;
    }

    APawn* AggressorPawn = Cast<APawn>(Aggressor);
    if (!AggressorPawn)
    {
       AggressorPawn = Aggressor->GetInstigator();
    }
    if (!AggressorPawn)
    {
       AggressorPawn = Cast<APawn>(Aggressor->GetOwner());
    }
    if (!AggressorPawn)
    {
       if (const AController* AggressorController = Cast<AController>(Aggressor))
       {
          AggressorPawn = AggressorController->GetPawn();
       }
    }
    if (!IsLivingPlayerTarget(AggressorPawn))
    {
       return false;
    }

    CurrentAggro = FMath::Min(MaxAggro, CurrentAggro + AggroAmount);
    SetTargetActor(AggressorPawn);
    return true;
}

void UASMonsterTargetingComponent::UpdateAggro(float DeltaSeconds)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || DeltaSeconds <= 0.0f || !HasValidTarget())
    {
       return;
    }

    const APawn* TargetPawn = Cast<APawn>(TargetActor);
    if (!IsLivingPlayerTarget(TargetPawn))
    {
       ClearTarget();
       return;
    }

    if (FVector::Dist2D(GetOwner()->GetActorLocation(), TargetPawn->GetActorLocation()) <= LoseTargetRadius)
    {
       return;
    }

    CurrentAggro = FMath::Max(0.0f, CurrentAggro - (AggroDecayPerSecond * DeltaSeconds));

    if (CurrentAggro <= 0.0f)
    {
       ClearTarget();
    }
}

bool UASMonsterTargetingComponent::IsLivingPlayerTarget(const APawn* Pawn) const
{
    if (!IsValid(Pawn) || !Pawn->IsPlayerControlled())
    {
       return false;
    }

    const IGenericTeamAgentInterface* OwnerTeam = Cast<IGenericTeamAgentInterface>(GetOwner());
    if (!OwnerTeam || OwnerTeam->GetTeamAttitudeTowards(*Pawn) != ETeamAttitude::Hostile)
    {
       return false;
    }

    const IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(Pawn);
    const UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemOwner ? AbilitySystemOwner->GetAbilitySystemComponent() : nullptr;
    if (!AbilitySystemComponent || AbilitySystemComponent->GetNumericAttribute(UASHealthAttributeSet::GetHealthAttribute()) <= 0.0f)
    {
       return false;
    }

    // 플레이어가 제공하는 동적 타깃 가능 상태를 우선한다. 다운/사망은 체력값과 별개로 즉시 제외한다.
    return !Pawn->GetClass()->ImplementsInterface(UASMonsterTargetable::StaticClass())
       || IASMonsterTargetable::Execute_IsMonsterTargetable(const_cast<APawn*>(Pawn));
}

bool UASMonsterTargetingComponent::HasValidTarget() const
{
    return IsLivingPlayerTarget(Cast<APawn>(TargetActor));
}

void UASMonsterTargetingComponent::SetTargetActor(AActor* NewTarget)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || TargetActor == NewTarget)
    {
       return;
    }

    AActor* PreviousTarget = TargetActor;
    TargetActor = NewTarget;
    OnTargetChanged.Broadcast(PreviousTarget, TargetActor);
}

void UASMonsterTargetingComponent::ClearTarget()
{
    CurrentAggro = 0.0f;
    SetTargetActor(nullptr);
}

void UASMonsterTargetingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UASMonsterTargetingComponent, TargetActor);
}

void UASMonsterTargetingComponent::OnRep_TargetActor(AActor* PreviousTarget)
{
    OnTargetChanged.Broadcast(PreviousTarget, TargetActor);
}