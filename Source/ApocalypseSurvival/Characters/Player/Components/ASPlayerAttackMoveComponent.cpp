#include "Characters/Player/Components/ASPlayerAttackMoveComponent.h"

#include "AbilitySystemGlobals.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Characters/Player/Components/ASEquipmentComponent.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Abilities/ASGameplayAbility_CharacterPrimaryAttack.h"
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
	PendingAttackRequest.Reset();

	if (!UsesAttackMoveTargeting())
	{
		FASPrimaryAttackRequest Request;
		Request.TargetLocation = Destination;
		PendingAttackRequest = Request;
		State = EASAttackMoveState::Idle;
		if (IsPrimaryAttackActive())
		{
			return true;
		}
		return TryStartAttack(nullptr, Destination);
	}

	// A로 이미 활성화된 Primary Attack은 다음 좌클릭의 LocalInputConfirm을 기다린다.
	// 사거리 안의 대상은 이 시점에 넣어 BP 조준 표시와 대상 공격을 함께 유지한다.
	if (AActor* Target = FindBestTarget(); Target && IsTargetInAttackRange(Target))
	{
		FASPrimaryAttackRequest Request;
		Request.Target = Target;
		Request.TargetLocation = Target->GetActorLocation();
		PendingAttackRequest = Request;
		State = EASAttackMoveState::Idle;
		if (IsPrimaryAttackActive())
		{
			return true;
		}
		return TryStartAttack(Target, Request.TargetLocation);
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
	PendingAttackRequest.Reset();
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

bool UASPlayerAttackMoveComponent::ConsumePendingAttackRequest(FASPrimaryAttackRequest& OutRequest)
{
	if (!PendingAttackRequest.IsSet())
	{
		return false;
	}

	OutRequest = PendingAttackRequest.GetValue();
	PendingAttackRequest.Reset();
	return true;
}

void UASPlayerAttackMoveComponent::ClearPendingAttackRequest()
{
	PendingAttackRequest.Reset();
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
		if (CandidateToCursorSquared <= FMath::Square(CursorAcquireRadius)
			&& CandidateToCursorSquared < CursorDistanceSquared)
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

bool UASPlayerAttackMoveComponent::IsPrimaryAttackActive() const
{
	const AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	const UASAbilitySystemComponent* ASC = Controller
		? Cast<UASAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Controller->GetPawn()))
		: nullptr;
	if (!ASC)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability
			&& Spec.Ability->GetClass()->IsChildOf(UASGameplayAbility_CharacterPrimaryAttack::StaticClass()))
		{
			return true;
		}
	}

	return false;
}

bool UASPlayerAttackMoveComponent::TryStartAttack(AActor* Target, const FVector& TargetLocation)
{
	AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	UASAbilitySystemComponent* ASC = Controller
		? Cast<UASAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Controller->GetPawn()))
		: nullptr;
	if (!ASC)
	{
		return false;
	}

	FASPrimaryAttackRequest Request;
	Request.Target = Target;
	Request.TargetLocation = TargetLocation;

	// A로 시작한 Primary Attack이 조준 상태로 살아 있으면 새로 발동하지 않는다.
	// 준비한 요청을 넣고 여기서만 좌클릭 확정을 보내므로, 적이 없을 때는
	// 바닥을 향한 공격이 발사되지 않는다.
	if (IsPrimaryAttackActive())
	{
		PendingAttackRequest = Request;
		ASC->LocalInputConfirm();
		return true;
	}

	FGameplayAbilitySpecHandle PrimaryAttackHandle;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass()->IsChildOf(UASGameplayAbility_CharacterPrimaryAttack::StaticClass()))
		{
			PrimaryAttackHandle = Spec.Handle;
			break;
		}
	}

	if (!PrimaryAttackHandle.IsValid())
	{
		return false;
	}

	PendingAttackRequest = Request;
	if (!ASC->TryActivateAbility(PrimaryAttackHandle))
	{
		PendingAttackRequest.Reset();
		return false;
	}

	return true;
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

	if (State == EASAttackMoveState::Attacking)
	{
		if (IsPrimaryAttackActive())
		{
			return;
		}
		State = CurrentTarget.IsValid() ? EASAttackMoveState::Chasing : EASAttackMoveState::Seeking;
	}

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
			// 바닥까지 도착했는데 공격할 적이 없으면 조준과 사거리 표시를 함께 종료한다.
			if (UASAbilitySystemComponent* ASC = Cast<UASAbilitySystemComponent>(
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn)))
			{
				ASC->LocalInputCancel();
			}
			CancelAttackMove();
			return;
		}

		RequestMove(CommandDestination, CurrentTime);
		return;
	}

	if (IsTargetInAttackRange(CurrentTarget.Get()))
	{
		Controller->StopMovement();
		if (TryStartAttack(CurrentTarget.Get(), CurrentTarget->GetActorLocation()))
		{
			State = EASAttackMoveState::Attacking;
		}
		else
		{
			State = EASAttackMoveState::Chasing;
		}
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
