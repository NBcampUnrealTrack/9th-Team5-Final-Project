#include "Characters/Player/Components/ASPlayerAttackMoveComponent.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Characters/Player/Components/ASEquipmentComponent.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "MapSystem/Vision/ASVisionFunctionLibrary.h"
#include "Skills/ASSkillAbility.h"

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
	if (AActor* Target = FindBestTarget(); Target && IsTargetInAttackRange(Target))
	{
		State = EASAttackMoveState::Idle;
		RequestAttack(Target, Target->GetActorLocation());
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
		State = CurrentTarget.IsValid() ? EASAttackMoveState::Chasing : EASAttackMoveState::Seeking;
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

AActor* UASPlayerAttackMoveComponent::FindBestTarget() const
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return nullptr;
	}

	const float AcquireRadius = FMath::Max(GetAttackRange(), MinimumAcquireRadius);
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASAttackMoveAcquire), false, Pawn);
	World->OverlapMultiByObjectType(Overlaps, Pawn->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(AcquireRadius), Params);

	AActor* CursorTarget = nullptr;
	float CursorDistanceSquared = TNumericLimits<float>::Max();
	AActor* PawnTarget = nullptr;
	float PawnDistanceSquared = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!IsValidAttackTarget(Candidate))
		{
			continue;
		}

		const float CandidateToPawnSquared = FVector::DistSquared2D(Candidate->GetActorLocation(), Pawn->GetActorLocation());
		if (CandidateToPawnSquared < PawnDistanceSquared)
		{
			PawnTarget = Candidate;
			PawnDistanceSquared = CandidateToPawnSquared;
		}

		const float CandidateToCursorSquared = FVector::DistSquared2D(Candidate->GetActorLocation(), CommandDestination);
		// 사거리 안 후보 중 클릭 지점에 가장 가까운 적을 항상 우선한다.
		if (CandidateToCursorSquared < CursorDistanceSquared)
		{
			CursorTarget = Candidate;
			CursorDistanceSquared = CandidateToCursorSquared;
		}
	}

	return CursorTarget ? CursorTarget : PawnTarget;
}

bool UASPlayerAttackMoveComponent::IsValidAttackTarget(const AActor* Candidate) const
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn || !Candidate || Candidate == Pawn || !UASVisionFunctionLibrary::IsActorVisibleToLocalTeam(Candidate))
	{
		return false;
	}

	if (const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(Candidate); Monster && Monster->IsDead())
	{
		return false;
	}

	return UASSkillAbility::IsValidSkillTarget(Candidate, Pawn, false);
}

bool UASPlayerAttackMoveComponent::IsTargetInAttackRange(const AActor* Target) const
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn || !Target)
	{
		return false;
	}

	const float Range = GetAttackRange() + AttackRangeTolerance;
	return FVector::DistSquared2D(Pawn->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(Range);
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

	if (!IsValidAttackTarget(CurrentTarget.Get()))
	{
		CurrentTarget.Reset();
	}

	if (!CurrentTarget.IsValid() && CurrentTime >= NextSearchTime)
	{
		CurrentTarget = FindBestTarget();
		NextSearchTime = CurrentTime + SearchInterval;
	}

	if (!CurrentTarget.IsValid())
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

	if (IsTargetInAttackRange(CurrentTarget.Get()))
	{
		Controller->StopMovement();
		State = EASAttackMoveState::Attacking;
		RequestAttack(CurrentTarget.Get(), CurrentTarget->GetActorLocation());
		return;
	}

	State = EASAttackMoveState::Chasing;
	RequestMove(CurrentTarget->GetActorLocation(), CurrentTime);
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
