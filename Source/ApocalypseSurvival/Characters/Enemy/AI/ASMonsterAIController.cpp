#include "Characters/Enemy/AI/ASMonsterAIController.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Components/StateTreeAIComponent.h"

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
	if (IsValid(StateTreeAIComponent) && StateTreeAIComponent->IsRunning())
	{
		StateTreeAIComponent->StopLogic(TEXT("Unpossessed"));
	}
	ControlledMonster.Reset();

	Super::OnUnPossess();
}

void AASMonsterAIController::TryStartAI()
{
	AASMonsterCharacter* Monster = ControlledMonster.Get();
	if (!HasAuthority() || !Monster || !Monster->IsMonsterInitialized() || Monster->IsDead() || IsAILogicRunning())
	{
		return;
	}

	StopWaitingForMonster();

	const UASMonsterDefinition* Definition = Monster->GetMonsterDefinition();
	if (!Definition || !Definition->StateTree)
	{
		UE_LOG(LogASMonsterAI, Warning, TEXT("%s: MonsterDefinition 또는 StateTree 가 없어 AI 를 시작하지 않는다."), *GetNameSafe(Monster));
		return;
	}

	StateTreeAIComponent->SetStateTree(Definition->StateTree);
	StateTreeAIComponent->StartLogic();
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
	if (IsValid(StateTreeAIComponent) && StateTreeAIComponent->IsRunning())
	{
		StateTreeAIComponent->StopLogic(Reason);
	}

	StopMovement();
	ClearCombatFocus();
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
