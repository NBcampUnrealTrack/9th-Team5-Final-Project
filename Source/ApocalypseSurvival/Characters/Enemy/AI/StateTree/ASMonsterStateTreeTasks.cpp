#include "Characters/Enemy/AI/StateTree/ASMonsterStateTreeTasks.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "BrainComponent.h"
#include "Characters/Enemy/AI/ASMonsterAIController.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/BossSpider/ASBossSpiderWebTrap.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "StateTreeExecutionContext.h"
#include "GAS/ASAbilityTags.h"

namespace
{
	/** AI 판단은 서버에서만 한다. 컴포넌트가 없거나 서버가 아니면 false. */
	bool IsServerComponent(const UActorComponent* Component)
	{
		return IsValid(Component) && Component->GetOwner() && Component->GetOwner()->HasAuthority();
	}

	float GetStateTreeTaskCapsuleRadius(const AActor* Actor)
	{
		const ACharacter* Character = Cast<ACharacter>(Actor);
		return Character ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
	}

	AASMonsterAIController* ResolveMonsterController(const FStateTreeExecutionContext& Context)
	{
		if (AASMonsterAIController* Controller = Cast<AASMonsterAIController>(Context.GetOwner()))
		{
			return Controller;
		}

		const UBrainComponent* Brain = Cast<UBrainComponent>(Context.GetOwner());
		return Brain ? Cast<AASMonsterAIController>(Brain->GetAIOwner()) : nullptr;
	}

	AASMonsterCharacter* ResolveMonster(const FStateTreeExecutionContext& Context)
	{
		const AASMonsterAIController* Controller = ResolveMonsterController(Context);
		return Controller ? Cast<AASMonsterCharacter>(Controller->GetPawn()) : nullptr;
	}
}

EStateTreeRunStatus FASMonsterAcquireTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsServerComponent(InstanceData.TargetingComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.TimeSinceSearch = 0.0f;
	return InstanceData.TargetingComponent->AcquireTarget() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FASMonsterAcquireTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsServerComponent(InstanceData.TargetingComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.TimeSinceSearch += DeltaTime;
	if (InstanceData.TimeSinceSearch < InstanceData.SearchInterval)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.TimeSinceSearch = 0.0f;
	return InstanceData.TargetingComponent->AcquireTarget() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FASMonsterRequestAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bWaitingForPatternAttack = false;
	if (!IsServerComponent(InstanceData.CombatComponent) || !IsValid(InstanceData.TargetingComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	// 타깃은 요청 순간의 값을 TargetingComponent 에서 직접 읽는다.
	AActor* Target = InstanceData.TargetingComponent->GetTargetActor();
	if (!IsValid(Target))
	{
		return EStateTreeRunStatus::Failed;
	}

	// 사거리, 패턴 쿨다운, 활성화 가능 여부는 CombatComponent 와 GAS 가 판단한다.
	if (!InstanceData.CombatComponent->TryRequestAttack(Target))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.CombatComponent->UsesAttackPatterns())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.bWaitingForPatternAttack = true;
	if (!InstanceData.CombatComponent->IsPatternAttackActive())
	{
		// 몽타주 없는 즉발 Ability는 TryActivateAbility 안에서 이미 끝난다.
		InstanceData.CombatComponent->NotifyPatternAttackFinished();
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FASMonsterRequestAttackTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.bWaitingForPatternAttack || !IsServerComponent(InstanceData.CombatComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.CombatComponent->IsPatternAttackActive())
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.CombatComponent->NotifyPatternAttackFinished();
	InstanceData.bWaitingForPatternAttack = false;
	return EStateTreeRunStatus::Succeeded;
}

namespace
{
	AActor* ResolvePatternRequestTarget(AActor* ExplicitTarget,
		const UASMonsterTargetingComponent* TargetingComponent)
	{
		return IsValid(ExplicitTarget)
			? ExplicitTarget
			: (IsValid(TargetingComponent) ? TargetingComponent->GetTargetActor() : nullptr);
	}
}

EStateTreeRunStatus FASMonsterRequestPatternByIdTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bRequestStarted = false;
	if (!IsValid(InstanceData.CombatComponent) || !IsValid(InstanceData.TargetingComponent))
	{
		if (AASMonsterCharacter* Monster = ResolveMonster(Context))
		{
			InstanceData.CombatComponent = Monster->GetCombatComponent();
			InstanceData.TargetingComponent = Monster->GetTargetingComponent();
		}
	}

	if (!IsServerComponent(InstanceData.CombatComponent) || !InstanceData.PatternId.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Target = ResolvePatternRequestTarget(InstanceData.TargetActor, InstanceData.TargetingComponent);
	if (!IsValid(Target)
		|| !InstanceData.CombatComponent->TryRequestPatternAttackById(InstanceData.PatternId, Target))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bRequestStarted = true;
	return InstanceData.CombatComponent->IsPatternAttackActive()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FASMonsterRequestPatternByIdTask::Tick(FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.bRequestStarted || !IsServerComponent(InstanceData.CombatComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	return InstanceData.CombatComponent->IsPatternAttackActive()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FASBossSpiderRequestCaptureChargeTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	Context.GetInstanceData(*this).bRequestStarted = false;
	return Tick(Context, 0.0f);
}

EStateTreeRunStatus FASBossSpiderRequestCaptureChargeTask::Tick(FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsServerComponent(InstanceData.CombatComponent)
		|| !InstanceData.CaptureChargePatternId.IsValid()
		|| !IsValid(InstanceData.TargetActor)
		|| !IsValid(InstanceData.SourceTrap)
		|| !InstanceData.SourceTrap->IsCaptureActiveFor(InstanceData.TargetActor))
	{
		return EStateTreeRunStatus::Failed;
	}

	const UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstanceData.TargetActor);
	if (!TargetASC || !TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_WebBound))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.bRequestStarted)
	{
		if (!InstanceData.CombatComponent->TryRequestPatternAttackById(
			InstanceData.CaptureChargePatternId, InstanceData.TargetActor))
		{
			// 현재 다른 패턴/공용 후딜/사거리 조건 때문에 거절된 경우 Payload를 유지하고 재시도한다.
			return EStateTreeRunStatus::Running;
		}
		InstanceData.bRequestStarted = true;
	}

	return InstanceData.CombatComponent->IsPatternAttackActive()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FASMonsterMoveIntoPatternRangeTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.TimeSinceMoveRequest = TNumericLimits<float>::Max();
	InstanceData.TimeWithoutProgress = 0.0f;
	InstanceData.LastDistanceToDesiredRange = TNumericLimits<float>::Max();
	InstanceData.bMovementModeInitialized = false;
	InstanceData.bRetreating = false;
	return Tick(Context, 0.0f);
}

EStateTreeRunStatus FASMonsterMoveIntoPatternRangeTask::Tick(FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.MonsterController))
	{
		InstanceData.MonsterController = ResolveMonsterController(Context);
	}
	if (!IsValid(InstanceData.CombatComponent) || !IsValid(InstanceData.TargetingComponent))
	{
		if (AASMonsterCharacter* Monster = ResolveMonster(Context))
		{
			InstanceData.CombatComponent = Monster->GetCombatComponent();
			InstanceData.TargetingComponent = Monster->GetTargetingComponent();
		}
	}

	if (!IsValid(InstanceData.MonsterController) || !InstanceData.MonsterController->HasAuthority()
		|| !IsServerComponent(InstanceData.CombatComponent) || !InstanceData.PatternId.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Target = ResolvePatternRequestTarget(InstanceData.TargetActor, InstanceData.TargetingComponent);
	if (!IsValid(Target))
	{
		return EStateTreeRunStatus::Failed;
	}

	float MinRange = 0.0f;
	float MaxRange = 0.0f;
	if (!InstanceData.CombatComponent->TryGetPatternRangeById(InstanceData.PatternId, MinRange, MaxRange))
	{
		return EStateTreeRunStatus::Failed;
	}

	APawn* MonsterPawn = InstanceData.MonsterController->GetPawn();
	if (!IsValid(MonsterPawn))
	{
		return EStateTreeRunStatus::Failed;
	}

	const float SurfaceDistance = FVector::Dist2D(MonsterPawn->GetActorLocation(), Target->GetActorLocation())
		- GetStateTreeTaskCapsuleRadius(MonsterPawn) - GetStateTreeTaskCapsuleRadius(Target);
	const float HalfRangeWidth = FMath::Max(0.0f, MaxRange - MinRange) * 0.5f;
	const float DesiredMinRange = MinRange
		+ FMath::Min(FMath::Max(0.0f, InstanceData.RetreatPadding), HalfRangeWidth);
	const float DesiredMaxRange = MaxRange
		- FMath::Min(FMath::Max(0.0f, InstanceData.ApproachPadding), HalfRangeWidth);

	if (!InstanceData.bMovementModeInitialized)
	{
		if (SurfaceDistance >= MinRange && SurfaceDistance <= MaxRange)
		{
			InstanceData.MonsterController->StopMovement();
			InstanceData.MonsterController->ClearCombatFocus();
			return EStateTreeRunStatus::Succeeded;
		}

		InstanceData.bRetreating = SurfaceDistance < MinRange;
		InstanceData.bMovementModeInitialized = true;
	}
	else if (InstanceData.bRetreating && SurfaceDistance > MaxRange)
	{
		InstanceData.bRetreating = false;
		InstanceData.LastDistanceToDesiredRange = TNumericLimits<float>::Max();
	}
	else if (!InstanceData.bRetreating && SurfaceDistance < MinRange)
	{
		InstanceData.bRetreating = true;
		InstanceData.LastDistanceToDesiredRange = TNumericLimits<float>::Max();
	}

	const bool bReachedDesiredRange = InstanceData.bRetreating
		? SurfaceDistance >= DesiredMinRange && SurfaceDistance <= MaxRange
		: SurfaceDistance <= DesiredMaxRange && SurfaceDistance >= MinRange;
	if (bReachedDesiredRange)
	{
		InstanceData.MonsterController->StopMovement();
		InstanceData.MonsterController->ClearCombatFocus();
		return EStateTreeRunStatus::Succeeded;
	}

	const float DistanceToDesiredRange = InstanceData.bRetreating
		? FMath::Max(0.0f, DesiredMinRange - SurfaceDistance)
		: FMath::Max(0.0f, SurfaceDistance - DesiredMaxRange);
	if (InstanceData.LastDistanceToDesiredRange == TNumericLimits<float>::Max()
		|| DistanceToDesiredRange <= InstanceData.LastDistanceToDesiredRange
			- FMath::Max(0.1f, InstanceData.ProgressTolerance))
	{
		InstanceData.LastDistanceToDesiredRange = DistanceToDesiredRange;
		InstanceData.TimeWithoutProgress = 0.0f;
	}
	else
	{
		InstanceData.TimeWithoutProgress += DeltaTime;
		if (InstanceData.TimeWithoutProgress >= FMath::Max(0.1f, InstanceData.MoveTimeout))
		{
			return EStateTreeRunStatus::Failed;
		}
	}

	InstanceData.TimeSinceMoveRequest += DeltaTime;
	if (InstanceData.TimeSinceMoveRequest >= FMath::Max(0.05f, InstanceData.RepathInterval))
	{
		InstanceData.TimeSinceMoveRequest = 0.0f;
		if (InstanceData.bRetreating)
		{
			FVector RetreatDirection = (MonsterPawn->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			if (RetreatDirection.IsNearlyZero())
			{
				RetreatDirection = -MonsterPawn->GetActorForwardVector().GetSafeNormal2D();
			}

			const float RetreatGoalRange = FMath::Min(MaxRange,
				DesiredMinRange + FMath::Max(0.0f, InstanceData.PathArrivalPadding));
			const float DesiredCenterDistance = RetreatGoalRange
				+ GetStateTreeTaskCapsuleRadius(MonsterPawn) + GetStateTreeTaskCapsuleRadius(Target);
			FVector RetreatLocation = Target->GetActorLocation() + RetreatDirection * DesiredCenterDistance;
			RetreatLocation.Z = MonsterPawn->GetActorLocation().Z;
			InstanceData.MonsterController->SetCombatFocus(Target);
			InstanceData.MonsterController->MoveToLocation(RetreatLocation, 25.0f, false);
		}
		else
		{
			InstanceData.MonsterController->ClearCombatFocus();
			const float ApproachGoalRange = FMath::Max(MinRange,
				DesiredMaxRange - FMath::Max(0.0f, InstanceData.PathArrivalPadding));
			InstanceData.MonsterController->MoveToActor(Target, FMath::Max(1.0f, ApproachGoalRange));
		}
	}
	return EStateTreeRunStatus::Running;
}

void FASMonsterMoveIntoPatternRangeTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (AASMonsterAIController* Controller = Context.GetInstanceData(*this).MonsterController)
	{
		Controller->StopMovement();
		Controller->ClearCombatFocus();
	}
}
