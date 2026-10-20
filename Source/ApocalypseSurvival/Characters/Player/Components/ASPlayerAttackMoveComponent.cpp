#include "Characters/Player/Components/ASPlayerAttackMoveComponent.h"

#include "Characters/Player/ASPlayerCharacter.h"
#include "Characters/Player/Components/ASEquipmentComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/World.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"

UASPlayerAttackMoveComponent::UASPlayerAttackMoveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

void UASPlayerAttackMoveComponent::ArmAttackMove()
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	if (!Controller || !Controller->IsLocalController() || !Controller->GetPawn())
	{
		return;
	}

	if (State == EASAttackMoveState::Armed)
	{
		CancelAttackMove();
		return;
	}

	CancelAttackMove();
	State = EASAttackMoveState::Armed;
}

bool UASPlayerAttackMoveComponent::ConfirmAttackMove(const FVector& Destination)
{
	if (State != EASAttackMoveState::Armed)
	{
		return false;
	}

	AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Controller || !Pawn)
	{
		CancelAttackMove();
		return false;
	}
	if (GetAttackRange() <= 0.0f)
	{
		CancelAttackMove();
		return false;
	}

	CommandDestination = Destination;
	CurrentTarget.Reset();

	if (!UsesAttackMoveTargeting())
	{
		State = EASAttackMoveState::Idle;
		RequestAttack(nullptr, Destination);
		return true;
	}

	// 사거리 안의 대상은 Controller에 공격 요청만 전달한다.
	// Ability 활성화와 조준 UI 종료는 Controller/Primary Attack이 담당한다.
	FVector TargetLocation;
	float TargetHitRadius = 0.0f;
	if (FindBestTarget() && RefreshCurrentTarget(TargetLocation, TargetHitRadius)
		&& IsTargetInAttackRange(TargetLocation, TargetHitRadius))
	{
		State = EASAttackMoveState::Idle;
		RequestAttack(CurrentTarget.GetActor(), TargetLocation);
		return true;
	}

	State = EASAttackMoveState::Seeking;
	NextSearchTime = 0.0;
	NextRepathTime = 0.0;
	UpdateTargetedCommand(Controller->GetWorld()->GetTimeSeconds());
	return true;
}

void UASPlayerAttackMoveComponent::CancelAttackMove()
{
	State = EASAttackMoveState::Idle;
	CurrentTarget.Reset();
	NextSearchTime = 0.0;
	NextRepathTime = 0.0;
}

void UASPlayerAttackMoveComponent::NotifyExternalMoveRequested()
{
	if (State != EASAttackMoveState::Idle)
	{
		CancelAttackMove();
	}
}

void UASPlayerAttackMoveComponent::NotifyAttackFinished()
{
	if (State == EASAttackMoveState::Attacking)
	{
		// A + 클릭은 자동 평타 토글이 아니라 단일 공격 명령이다.
		// 첫 공격이 끝나면 추적·반복 공격을 재개하지 않는다.
		CancelAttackMove();
	}
}

void UASPlayerAttackMoveComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (State != EASAttackMoveState::Seeking && State != EASAttackMoveState::Chasing
		&& State != EASAttackMoveState::Attacking)
	{
		return;
	}

	AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const AASPlayerCharacter* Character = Controller ? Cast<AASPlayerCharacter>(Controller->GetPawn()) : nullptr;
	if (!Controller || !Controller->IsLocalController() || !Character || Character->IsDead())
	{
		CancelAttackMove();
		return;
	}

	UpdateTargetedCommand(Controller->GetWorld()->GetTimeSeconds());
}

bool UASPlayerAttackMoveComponent::UsesAttackMoveTargeting() const
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const AASPlayerCharacter* Character = Controller ? Cast<AASPlayerCharacter>(Controller->GetPawn()) : nullptr;
	const UASEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	const UASItemInstance* Weapon = Equipment ? Equipment->GetEquippedItem() : nullptr;
	const UASItemDefinition* Definition = Weapon ? Weapon->GetItemDefinition() : nullptr;
	const UASRangedAttackProfile* RangedProfile = Definition ? Cast<UASRangedAttackProfile>(Definition->AttackProfile) : nullptr;

	return !RangedProfile || RangedProfile->bUseAttackMoveTargeting;
}

float UASPlayerAttackMoveComponent::GetAttackRange() const
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const AASPlayerCharacter* Character = Controller ? Cast<AASPlayerCharacter>(Controller->GetPawn()) : nullptr;
	const UASEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	const UASItemInstance* Weapon = Equipment ? Equipment->GetEquippedItem() : nullptr;
	const UASItemDefinition* Definition = Weapon ? Weapon->GetItemDefinition() : nullptr;
	return Definition ? FMath::Max(0.0f, Definition->AttackRange) : 0.0f;
}

bool UASPlayerAttackMoveComponent::FindBestTarget()
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return false;
	}

	const float AcquireRadius = FMath::Max(GetAttackRange(), MinimumAcquireRadius);
	const UASCombatTargetSubsystem* CombatTargets = World->GetSubsystem<UASCombatTargetSubsystem>();
	if (!CombatTargets)
	{
		CurrentTarget.Reset();
		return false;
	}

	FASCombatTargetQuery Query;
	Query.SearchOrigin = Pawn->GetActorLocation();
	Query.PreferredLocation = CommandDestination;
	Query.SearchRadius = AcquireRadius;
	Query.Instigator = Pawn;
	Query.Domain = EASCombatTargetDomain::All;
	Query.bRequireVisibleToLocalTeam = true;
	return CombatTargets->FindBestTarget(Query, CurrentTarget);
}

bool UASPlayerAttackMoveComponent::RefreshCurrentTarget(FVector& OutLocation, float& OutHitRadius)
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	const UASCombatTargetSubsystem* CombatTargets =
		World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!Pawn || !CombatTargets)
	{
		CurrentTarget.Reset();
		return false;
	}

	FASCombatTargetQuery Query;
	Query.Instigator = Pawn;
	Query.Domain = EASCombatTargetDomain::All;
	Query.bRequireVisibleToLocalTeam = true;
	if (!CombatTargets->RefreshTarget(Query, CurrentTarget))
	{
		return false;
	}

	OutLocation = CurrentTarget.GetLocation();
	OutHitRadius = CurrentTarget.GetHitRadius();
	return true;
}

bool UASPlayerAttackMoveComponent::IsTargetInAttackRange(
	const FVector& TargetLocation,
	const float TargetHitRadius) const
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return false;
	}

	const float Range = GetAttackRange() + AttackRangeTolerance + FMath::Max(0.0f, TargetHitRadius);
	return FVector::DistSquared2D(Pawn->GetActorLocation(), TargetLocation) <= FMath::Square(Range);
}

void UASPlayerAttackMoveComponent::RequestAttack(AActor* Target, const FVector& TargetLocation)
{
	FASPrimaryAttackRequest Request;
	Request.Target = Target;
	Request.TargetLocation = TargetLocation;
	OnAttackRequested.Broadcast(Request);
}

void UASPlayerAttackMoveComponent::UpdateTargetedCommand(const double CurrentTime)
{
	AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Controller || !Pawn)
	{
		CancelAttackMove();
		return;
	}

	if (State == EASAttackMoveState::Attacking) return;

	FVector TargetLocation;
	float TargetHitRadius = 0.0f;
	bool bHasTarget = RefreshCurrentTarget(TargetLocation, TargetHitRadius);
	if (!bHasTarget && CurrentTime >= NextSearchTime)
	{
		FindBestTarget();
		bHasTarget = RefreshCurrentTarget(TargetLocation, TargetHitRadius);
		NextSearchTime = CurrentTime + SearchInterval;
	}

	if (!bHasTarget)
	{
		State = EASAttackMoveState::Seeking;
		if (FVector::DistSquared2D(Pawn->GetActorLocation(), CommandDestination)
			<= FMath::Square(DestinationAcceptanceRadius))
		{
			CancelAttackMove();
			return;
		}

		RequestMove(CommandDestination, CurrentTime);
		return;
	}

	if (IsTargetInAttackRange(TargetLocation, TargetHitRadius))
	{
		Controller->StopMovement();
		State = EASAttackMoveState::Attacking;
		RequestAttack(CurrentTarget.GetActor(), TargetLocation);
		return;
	}

	State = EASAttackMoveState::Chasing;
	RequestMove(TargetLocation, CurrentTime);
}

void UASPlayerAttackMoveComponent::RequestMove(const FVector& Destination, const double CurrentTime)
{
	if (CurrentTime < NextRepathTime)
	{
		return;
	}

	if (AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner()))
	{
		Controller->RequestMoveTo(Destination);
		NextRepathTime = CurrentTime + RepathInterval;
	}
}
