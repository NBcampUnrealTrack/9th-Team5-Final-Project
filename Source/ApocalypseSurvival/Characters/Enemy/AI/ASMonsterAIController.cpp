#include "Characters/Enemy/AI/ASMonsterAIController.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/Framework/ASGameMode.h"
#include "Core/Framework/ASGameState.h"
#include "Engine/World.h"
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

void AASMonsterAIController::NotifyMonsterDamaged(AActor* InstigatorActor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AASMonsterCharacter* Monster = ControlledMonster.Get())
	{
		if (UASMonsterTargetingComponent* Targeting = Monster->GetTargetingComponent(); Targeting && Targeting->ForceAggro(InstigatorActor))
		{
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
	MoveTarget.Reset();
}

void AASMonsterAIController::UpdateAggroAndMovement()
{
	AASMonsterCharacter* Monster = ControlledMonster.Get();
	UASMonsterTargetingComponent* Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	if (!HasAuthority() || !Monster || Monster->IsDead() || !Targeting)
	{
		return;
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

	const bool bHasTarget = bNightAggressive
		? (Targeting->ValidateTarget(true) || Targeting->AcquireTargetAnywhere())
		: (Targeting->ValidateTarget() || Targeting->AcquireTarget());

	AActor* Target = bHasTarget ? Targeting->GetTargetActor() : nullptr;
	if (!IsValid(Target))
	{
		StopStateTree(TEXT("NoTarget"));
		StopMovement();
		ClearCombatFocus();
		MoveTarget.Reset();
		return;
	}

	// 타깃을 얻은 뒤에만 StateTree를 시작한다. 빈 TargetActor가 Move To에 전달되는 것을 차단한다.
	StartStateTree();

	if (MoveTarget != Target || GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		const UASMonsterCombatComponent* Combat = Monster->GetCombatComponent();
		const float AcceptanceRadius = Combat ? FMath::Max(Combat->GetAttackRange(), 1.0f) : 100.0f;
		MoveToActor(Target, AcceptanceRadius);
		MoveTarget = Target;
	}
}

void AASMonsterAIController::HandleNightStarted(int32 Day, bool bBossNight)
{
	bNightAggressive = true;
	UpdateAggroAndMovement();
}

void AASMonsterAIController::HandleNightEnded(int32 Day)
{
	bNightAggressive = false;
	UpdateAggroAndMovement();
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
