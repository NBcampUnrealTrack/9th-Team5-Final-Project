#include "Characters/Enemy/AI/ASMonsterAIController.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/Framework/ASGameMode.h"
#include "Core/Framework/ASGameState.h"
#include "Engine/World.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogASMonsterAI, Log, All);

AASMonsterAIController::AASMonsterAIController()
{
    StateTreeAIComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
    // 컴포넌트의 BeginPlay 자동 시작도 끈다. 시작 시점은 TryStartAI 가 정한다.
    StateTreeAIComponent->SetStartLogicAutomatically(false);

    bStartAILogicOnPossess = false;
    // EQS 가 컨트롤러의 위치를 폰 기준으로 읽으려면 폰에 붙어 있어야 한다.
    bAttachToPawn = true;
}

FGenericTeamId AASMonsterAIController::GetGenericTeamId() const
{
    if (const IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(GetPawn()))
    {
       return PawnTeam->GetGenericTeamId();
    }

    return FGenericTeamId::NoTeam;
}

bool AASMonsterAIController::IsAILogicRunning() const
{
    return IsValid(StateTreeAIComponent) && StateTreeAIComponent->IsRunning();
}

void AASMonsterAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // AI 는 서버에서만 판단한다. 컨트롤러는 어차피 서버에만 있지만 시작 경로도 명시적으로 막아 둔다.
    if (!HasAuthority())
    {
       return;
    }

    StopWaitingForMonster();
    ControlledMonster = Cast<AASMonsterCharacter>(InPawn);

    AASMonsterCharacter* Monster = ControlledMonster.Get();
    if (!Monster)
    {
       return;
    }

    if (Monster->IsMonsterInitialized())
    {
       TryStartAI();
    }
    else
    {
       // 컨트롤러가 몬스터의 BeginPlay 보다 먼저 빙의하는 경우. 초기화가 끝나는 순간 시작한다.
       MonsterInitializedHandle = Monster->OnMonsterInitialized.AddUObject(this, &AASMonsterAIController::TryStartAI);
    }
}

void AASMonsterAIController::OnUnPossess()
{
    StopWaitingForMonster();
    StopAggroLoop();
    StopStateTree(TEXT("Unpossessed"));
    ControlledMonster.Reset();

    Super::OnUnPossess();
}

void AASMonsterAIController::TryStartAI()
{
    AASMonsterCharacter* Monster = ControlledMonster.Get();
    if (!HasAuthority() || !Monster || !Monster->IsMonsterInitialized() || Monster->IsDead())
    {
       return;
    }

    StopWaitingForMonster();
    if (Monster->HasSpawnHome())
    {
       if (UASMonsterTargetingComponent* Targeting = Monster->GetTargetingComponent())
       {
          const UASMonsterDefinition* Definition = Monster->GetMonsterDefinition();
          Targeting->Configure(Monster->GetDayDetectionRadius(), Monster->GetDayLoseTargetRadius(),
             Definition ? Definition->AggroDecayPerSecond : 25.0f);
       }
    }
    StartAggroLoop();
    UpdateAggroAndMovement();
}

void AASMonsterAIController::StartStateTree()
{
    AASMonsterCharacter* Monster = ControlledMonster.Get();
    if (!HasAuthority() || !Monster || Monster->IsDead() || IsAILogicRunning())
    {
       return;
    }

    const UASMonsterDefinition* Definition = Monster->GetMonsterDefinition();
    if (!Definition || !Definition->StateTree)
    {
       UE_LOG(LogASMonsterAI, Warning, TEXT("%s: MonsterDefinition 또는 StateTree 가 없어 AI 를 시작하지 않는다."), *GetNameSafe(Monster));
       return;
    }

    StateTreeAIComponent->SetStateTree(Definition->StateTree);
    StateTreeAIComponent->StartLogic();
}

void AASMonsterAIController::StopStateTree(const FString& Reason)
{
    if (IsValid(StateTreeAIComponent) && StateTreeAIComponent->IsRunning())
    {
       StateTreeAIComponent->StopLogic(Reason);
    }
}

void AASMonsterAIController::StopWaitingForMonster()
{
    if (MonsterInitializedHandle.IsValid())
    {
       if (AASMonsterCharacter* Monster = ControlledMonster.Get())
       {
          Monster->OnMonsterInitialized.Remove(MonsterInitializedHandle);
       }
       MonsterInitializedHandle.Reset();
    }
}

void AASMonsterAIController::StopAI(const FString& Reason)
{
    if (!HasAuthority())
    {
       return;
    }

    StopWaitingForMonster();
    StopAggroLoop();
    StopStateTree(Reason);

    StopMovement();
    ClearCombatFocus();
}

void AASMonsterAIController::NotifyMonsterDamaged(AActor* InstigatorActor, float AggroAmount)
{
    if (!HasAuthority())
    {
       return;
    }

    if (AASMonsterCharacter* Monster = ControlledMonster.Get())
    {
       if (UASMonsterTargetingComponent* Targeting = Monster->GetTargetingComponent(); Targeting && Targeting->ForceAggro(InstigatorActor, AggroAmount))
       {
          // 이전 타깃을 놓고 귀환하던 상태라도, 피격은 새 전투 시작으로 취급한다.
          // 다음 갱신에서 Spawn Leash를 넘었다면 기존 귀환 규칙이 우선한다.
          bReturningToSpawn = false;
          bHasEngagedDayTarget = true;
          RestoreMoveSpeed();
          UpdateAggroAndMovement();
       }
    }
}

void AASMonsterAIController::NotifyMonsterHitReact()
{
    if (!HasAuthority())
    {
       return;
    }

    StopMovement();
    ClearCombatFocus();
    MoveTarget.Reset();
}

void AASMonsterAIController::StartAggroLoop()
{
    if (!HasAuthority() || !GetWorld())
    {
       return;
    }

    if (AASGameState* GameState = GetWorld()->GetGameState<AASGameState>())
    {
       bNightAggressive = GameState->GetCurrentPhase() == EGamePhase::Night;
    }

    if (AASGameMode* GameMode = GetWorld()->GetAuthGameMode<AASGameMode>())
    {
       if (!NightStartedHandle.IsValid())
       {
          NightStartedHandle = GameMode->OnNightStarted.AddUObject(this, &AASMonsterAIController::HandleNightStarted);
       }
       if (!NightEndedHandle.IsValid())
       {
          NightEndedHandle = GameMode->OnNightEnded.AddUObject(this, &AASMonsterAIController::HandleNightEnded);
       }
    }

    if (!GetWorld()->GetTimerManager().IsTimerActive(AggroUpdateTimerHandle))
    {
       GetWorld()->GetTimerManager().SetTimer(AggroUpdateTimerHandle, this, &AASMonsterAIController::UpdateAggroAndMovement,
          AggroUpdateInterval, true);
    }
}

void AASMonsterAIController::StopAggroLoop()
{
    if (UWorld* World = GetWorld())
    {
       World->GetTimerManager().ClearTimer(AggroUpdateTimerHandle);
       if (AASGameMode* GameMode = World->GetAuthGameMode<AASGameMode>())
       {
          if (NightStartedHandle.IsValid())
          {
             GameMode->OnNightStarted.Remove(NightStartedHandle);
          }
          if (NightEndedHandle.IsValid())
          {
             GameMode->OnNightEnded.Remove(NightEndedHandle);
          }
       }
    }

    NightStartedHandle.Reset();
    NightEndedHandle.Reset();
    RestoreMoveSpeed();
    MoveTarget.Reset();
    bReturningToSpawn = false;
    bContinueTrackingNightTarget = false;
    bHasEngagedDayTarget = false;
}

void AASMonsterAIController::UpdateAggroAndMovement()
{
    AASMonsterCharacter* Monster = ControlledMonster.Get();
    UASMonsterTargetingComponent* Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
    if (!HasAuthority() || !Monster || Monster->IsDead() || !Targeting)
    {
       return;
    }

    // 낮에는 필드 스폰 몬스터만 일반 탐지 거리에서 반응한다.
    // 단, 밤에 한 번이라도 추격했던 몬스터는 사망 전까지 전장에 남는다.
    if (!bNightAggressive && !bContinueTrackingNightTarget && !Monster->CanAggroDuringDay())
    {
       Targeting->ClearTarget();
       StopStateTree(TEXT("DayIdle"));
       StopMovement();
       ClearCombatFocus();
       MoveTarget.Reset();
       return;
    }

    // 낮 필드 몬스터는 홈에서 LeashRadius를 넘으면 즉시 전투를 끊고 귀환한다.
    // 밤에는 거리 제한 추격 규칙이 우선이므로 귀환시키지 않는다.
    if (!bNightAggressive && !bContinueTrackingNightTarget
       && (bReturningToSpawn || Monster->IsOutsideSpawnLeash()))
    {
       if (UpdateReturnToSpawn(*Monster, *Targeting))
       {
          return;
       }
    }

    if (!bNightAggressive && !bContinueTrackingNightTarget)
    {
       Targeting->UpdateAggro(AggroUpdateInterval);
    }

    // 타이머가 0.25초마다 MoveToActor를 재요청하므로, 태그를 여기서 확인해야 경직 중 이동이 재개되지 않는다.
    if (Monster->IsHitReacting())
    {
       StopMovement();
       ClearCombatFocus();
       MoveTarget.Reset();
       return;
    }

    // 패턴 Ability가 선딜·돌진처럼 직접 이동을 제어하는 동안에는 주기적인 MoveTo 요청으로 덮어쓰지 않는다.
    // StopMovement는 PathFollowing만 중단하며, 돌진 Ability의 Root Motion 이동은 Ability 쪽에서 계속 제어한다.
    if (const UASMonsterCombatComponent* Combat = Monster->GetCombatComponent(); Combat && Combat->IsPatternAttackActive())
    {
       StopMovement();
       MoveTarget.Reset();
       return;
    }

    bool bHasTarget = false;
    if (bNightAggressive)
    {
       bHasTarget = Targeting->ValidateTarget(true) || Targeting->AcquireTargetAnywhere();
    }
    else if (bContinueTrackingNightTarget)
    {
       // 기존 타깃이 죽거나 Pawn이 교체되어도 홈으로 돌아가지 않고, 다른 살아 있는 플레이어를 계속 찾는다.
       bHasTarget = Targeting->ValidateTarget(true) || Targeting->AcquireTargetAnywhere();
    }
    else
    {
       bHasTarget = Targeting->ValidateTarget() || Targeting->AcquireTarget();
    }

    AActor* Target = bHasTarget ? Targeting->GetTargetActor() : nullptr;
    if (!IsValid(Target))
    {
       StateTreeTarget.Reset();
       // 플레이어가 추적 해제 거리 밖으로 먼저 빠진 경우에도 전투를 끝내고 홈으로 복귀한다.
       if (!bNightAggressive && !bContinueTrackingNightTarget && bHasEngagedDayTarget && Monster->HasSpawnHome()
          && FVector::Dist2D(Monster->GetActorLocation(), Monster->GetSpawnHomeLocation()) > Monster->GetReturnAcceptanceRadius())
       {
          UpdateReturnToSpawn(*Monster, *Targeting);
          return;
       }

       bHasEngagedDayTarget = false;
       StopMovement();
       ClearCombatFocus();
       MoveTarget.Reset();
       return;
    }

    if (bNightAggressive)
    {
       // 밤에 실제 플레이어를 한 번이라도 찾은 순간부터 이 몬스터의 홈 복귀는 영구적으로 비활성화한다.
       bContinueTrackingNightTarget = true;
    }
    else if (!bContinueTrackingNightTarget)
    {
       bHasEngagedDayTarget = true;
    }

    // 플레이어 부활은 기존 Pawn을 파괴하고 새 Pawn을 빙의한다. 이동 루프는 새 타깃을
    // 바로 따라가지만, 실행 중인 StateTree의 공격 Task는 이전 Pawn 기준으로 남을 수 있다.
    // 대상 Actor가 바뀐 경우에만 트리를 재시작해 공격 조건을 새 Pawn 기준으로 재평가한다.
    if (StateTreeTarget != Target)
    {
       StateTreeTarget = Target;
       StopStateTree(TEXT("TargetChanged"));
    }

    // 타깃을 얻은 뒤에만 StateTree를 시작한다. 빈 TargetActor가 Move To에 전달되는 것을 차단한다.
    StartStateTree();

    const UASMonsterDefinition* Definition = Monster->GetMonsterDefinition();
    const float RetreatDistance = Definition ? FMath::Max(0.0f, Definition->RetreatDistance) : 0.0f;
    const float CurrentDistance = FVector::Dist2D(Monster->GetActorLocation(), Target->GetActorLocation());
    if (RetreatDistance > 0.0f && CurrentDistance < RetreatDistance)
    {
       // 원거리형은 추격 AI가 계속 타깃에게 붙어 버리면 핵심 패턴의 회피 설계가 무너진다.
       // NavMesh가 없는 방향은 MoveToLocation이 실패하므로 다음 갱신에서 다른 지점으로 다시 시도한다.
       FVector RetreatDirection = (Monster->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
       if (RetreatDirection.IsNearlyZero())
       {
          RetreatDirection = -Monster->GetActorForwardVector().GetSafeNormal2D();
       }

       const float DesiredDistance = Definition
          ? FMath::Max(RetreatDistance, Definition->PreferredCombatDistance)
          : RetreatDistance;
       const FVector RetreatLocation = Monster->GetActorLocation()
          + RetreatDirection * FMath::Max(100.0f, DesiredDistance - CurrentDistance);
       SetCombatFocus(Target);
       if (GetMoveStatus() != EPathFollowingStatus::Moving)
       {
          MoveToLocation(RetreatLocation, 75.0f, false);
       }
       MoveTarget.Reset();
       return;
    }

    if (MoveTarget != Target || GetMoveStatus() != EPathFollowingStatus::Moving)
    {
       const UASMonsterCombatComponent* Combat = Monster->GetCombatComponent();
       float PatternApproachRange = 0.0f;
       const bool bUsesAutonomousPatternApproach = Combat
          && Combat->TryGetAutonomousPatternApproachRange(PatternApproachRange);
       const float DefaultAcceptanceRadius = bUsesAutonomousPatternApproach
          ? FMath::Max(PatternApproachRange, 1.0f)
          : (Combat ? FMath::Max(Combat->GetAttackRange(), 1.0f) : 100.0f);
       // PatternSet 사용 몬스터는 Definition의 단일 거리 대신 활성 일반 Pattern의 유효 사거리까지 접근한다.
       const float AcceptanceRadius = !bUsesAutonomousPatternApproach && Definition
          && Definition->PreferredCombatDistance > 0.0f
          ? Definition->PreferredCombatDistance
          : DefaultAcceptanceRadius;
       MoveToActor(Target, AcceptanceRadius);
       MoveTarget = Target;
    }
}

void AASMonsterAIController::HandleNightStarted(int32 Day, bool bBossNight)
{
    RestoreMoveSpeed();
    bNightAggressive = true;
    bReturningToSpawn = false;
    bHasEngagedDayTarget = false;
    UpdateAggroAndMovement();
}

void AASMonsterAIController::HandleNightEnded(int32 Day)
{
    RestoreMoveSpeed();
    bNightAggressive = false;
    bReturningToSpawn = false;

    if (AASMonsterCharacter* Monster = ControlledMonster.Get())
    {
       if (UASMonsterTargetingComponent* Targeting = Monster->GetTargetingComponent())
       {
          // 밤에 이미 추격했거나, 종료 직전에 타깃을 잡고 있으면 이후 낮에도 홈 복귀 없이 추적한다.
          bContinueTrackingNightTarget = bContinueTrackingNightTarget || Targeting->ValidateTarget(true);
          bHasEngagedDayTarget = false;
          if (bContinueTrackingNightTarget)
          {
             UpdateAggroAndMovement();
             return;
          }
       }
    }

    bHasEngagedDayTarget = false;
    StopStateTree(TEXT("NightEnded"));
    StopMovement();
    ClearCombatFocus();
    MoveTarget.Reset();
}

bool AASMonsterAIController::UpdateReturnToSpawn(AASMonsterCharacter& Monster, UASMonsterTargetingComponent& Targeting)
{
    if (!Monster.HasSpawnHome())
    {
       bReturningToSpawn = false;
       return false;
    }

    const bool bWasReturningToSpawn = bReturningToSpawn;
    bReturningToSpawn = true;
    Targeting.ClearTarget();
    StopStateTree(TEXT("ReturnToSpawn"));
    ClearCombatFocus();
    MoveTarget.Reset();
    ApplyReturnMoveSpeed(Monster);
    RestoreReturningHealth(Monster);

    const float DistanceToHome = FVector::Dist2D(Monster.GetActorLocation(), Monster.GetSpawnHomeLocation());
    if (DistanceToHome <= Monster.GetReturnAcceptanceRadius())
    {
       // 귀환을 끝낸 뒤에는 전투 중 상태를 남기지 않는다. 다음 갱신부터 일반 대기 몬스터처럼
       // DetectionRadius 안의 플레이어만 새로 탐색한다.
       Targeting.ClearTarget();
       bReturningToSpawn = false;
       bHasEngagedDayTarget = false;
       RestoreMoveSpeed();
       StopMovement();
       ClearCombatFocus();
       MoveTarget.Reset();
       // 귀환 중 멈췄던 StateTree를 대기/타깃 탐색 상태로 즉시 재가동한다.
       // 이후에는 StateTree와 컨트롤러 양쪽 모두 새 타깃을 다시 탐색한다.
       StartStateTree();
       return true;
    }

    if (!bWasReturningToSpawn || GetMoveStatus() != EPathFollowingStatus::Moving)
    {
       // 기본값인 bStopOnOverlap=true는 캡슐 반경만큼 일찍 멈춘다. 그러면 겉으로는 도착해도
       // 위 DistanceToHome 완료 조건을 영원히 통과하지 못해 bReturningToSpawn이 고착된다.
       MoveToLocation(Monster.GetSpawnHomeLocation(), Monster.GetReturnAcceptanceRadius(), false);
    }
    return true;
}

void AASMonsterAIController::ApplyReturnMoveSpeed(AASMonsterCharacter& Monster)
{
    if (bHasCachedMoveSpeedForReturn)
    {
       return;
    }

    if (UCharacterMovementComponent* Movement = Monster.GetCharacterMovement())
    {
       CachedMoveSpeedForReturn = Movement->MaxWalkSpeed;
       bHasCachedMoveSpeedForReturn = true;
       Movement->MaxWalkSpeed = CachedMoveSpeedForReturn * Monster.GetReturnMoveSpeedMultiplier();
    }
}

void AASMonsterAIController::RestoreMoveSpeed()
{
    if (!bHasCachedMoveSpeedForReturn)
    {
       return;
    }

    if (AASMonsterCharacter* Monster = ControlledMonster.Get())
    {
       if (UCharacterMovementComponent* Movement = Monster->GetCharacterMovement())
       {
          Movement->MaxWalkSpeed = CachedMoveSpeedForReturn;
       }
    }

    bHasCachedMoveSpeedForReturn = false;
    CachedMoveSpeedForReturn = 0.0f;
}

void AASMonsterAIController::RestoreReturningHealth(AASMonsterCharacter& Monster) const
{
    UASAbilitySystemComponent* AbilitySystemComponent = Monster.GetASAbilitySystemComponent();
    if (!AbilitySystemComponent)
    {
       return;
    }

    const float CurrentHealth = AbilitySystemComponent->GetNumericAttribute(UASHealthAttributeSet::GetHealthAttribute());
    const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(UASHealthAttributeSet::GetMaxHealthAttribute());
    const float HealAmount = MaxHealth * Monster.GetReturnHealthPercentPerSecond() * AggroUpdateInterval;
    if (HealAmount > 0.0f && CurrentHealth < MaxHealth)
    {
       AbilitySystemComponent->SetNumericAttributeBase(UASHealthAttributeSet::GetHealthAttribute(),
          FMath::Min(CurrentHealth + HealAmount, MaxHealth));
    }
}

void AASMonsterAIController::SetCombatFocus(AActor* Target)
{
    if (!HasAuthority() || !IsValid(Target))
    {
       return;
    }

    if (AASMonsterCharacter* Monster = ControlledMonster.Get())
    {
       Monster->SetFaceControllerFocus(true);
    }
    SetFocus(Target);
}

void AASMonsterAIController::ClearCombatFocus()
{
    if (!HasAuthority())
    {
       return;
    }

    ClearFocus(EAIFocusPriority::Gameplay);
    if (AASMonsterCharacter* Monster = ControlledMonster.Get())
    {
       Monster->SetFaceControllerFocus(false);
    }
}
