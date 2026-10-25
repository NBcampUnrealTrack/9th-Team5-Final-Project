// Fill out your copyright notice in the Description page of Project Settings.


#include "ASSkillDash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Skills/Data/ASSkillData.h"
#include "TimerManager.h"
#include "GameFramework/RootMotionSource.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace ASSkillDashPrivate
{
	//충돌 확인 간격
	constexpr float HitCheckInterval = 0.02f;
}

UASSkillDash::UASSkillDash()
{
	//돌진이 끝날 때 직접 FinishSkill 호출
	bEndAbilityManually = true;
}

void UASSkillDash::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility,
	bool bWasCancelled)
{
	//취소 등으로 끝나도 돌진 정리 (태스크는 어빌리티 종료 시 자동으로 끝남)
	bDashStopped = true;
	bPlayingEndSection = false;
	DashTask = nullptr;
	UnlockRotation();

	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		Avatar->GetWorldTimerManager().ClearTimer(DashHitTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASSkillDash::OnSkillEffect_Implementation()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		FinishSkill(true);
		return;
	}

	FVector Aim = GetAimDirection().GetSafeNormal2D();
	if (Aim.IsNearlyZero())
	{
		Aim = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}

	//높이는 그대로, 조준 방향으로 DashDistance만큼
	const FVector StartLocation = Avatar->GetActorLocation();
	const FVector TargetLocation = StartLocation + Aim * DashDistance;

	bDashStopped = false;
	bPlayingEndSection = false;
	LastSweepLocation = StartLocation;
	DashHitTargets.Reset();

	//돌진 방향을 바라보고 회전 잠금 (노티파이 대기 중 우클릭으로 돌아갔어도 다시 맞춤)
	Avatar->SetActorRotation(FRotator(0.0f, Aim.Rotation().Yaw, 0.0f));
	LockRotation();

	//서버·클라이언트 모두 실행 (LocalPredicted라 내 화면에서도 바로 움직임)
	DashTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, NAME_None, TargetLocation, FMath::Max(DashDuration, 0.05f),
		false, MOVE_Walking,
		 true,
		 nullptr,
		//끝나면 미끄러지지 않고 멈춤
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);

	if (!DashTask)
	{
		FinishSkill(true);
		return;
	}

	DashTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UASSkillDash::OnDashFinished);
	//벽 등에 막혀 도착하지 못한 채 시간이 끝난 경우
	DashTask->OnTimedOut.AddDynamic(this, &UASSkillDash::OnDashFinished);
	DashTask->ReadyForActivation();

	//충돌 확인 (서버·클라이언트 모두: 내 화면에서도 부딪히는 순간 멈추게)
	Avatar->GetWorldTimerManager().SetTimer(DashHitTimerHandle,
		FTimerDelegate::CreateUObject(this, &UASSkillDash::CheckDashHit), ASSkillDashPrivate::HitCheckInterval, true);

	//시작하자마자 붙어 있는 적도 확인
	CheckDashHit();
}

void UASSkillDash::ApplySkillStats(const FASSkillStatsRow& Stats)
{
	Super::ApplySkillStats(Stats);

	DashDistance = Stats.DashDistance;
	DashDuration = Stats.DashDuration;
	DashHitRadius = Stats.DashHitRadius;
}

FASSkillIndicatorSettings UASSkillDash::GetIndicatorSettings_Implementation() const
{
	FASSkillIndicatorSettings Settings = Super::GetIndicatorSettings_Implementation();
	Settings.TargetingType = EASSkillTargetingType::Direction;
	Settings.Shape = EASSkillIndicatorShape::Line;
	Settings.Length = DashDistance;
	Settings.Width = DashHitRadius * 2.0f;
	Settings.RangeRingRadius = DashDistance;
	return Settings;
}

void UASSkillDash::OnDashFinished()
{
	StopDash();
}

void UASSkillDash::CheckDashHit()
{
	if (bDashStopped)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		StopDash();
		return;
	}

	const FVector SweepStart = LastSweepLocation;
	const FVector CurrentLocation = Avatar->GetActorLocation();

	if (bDrawDebug)
	{
		DrawDebugLine(World, SweepStart, CurrentLocation, FColor::Cyan, false, 1.0f, 0, 2.0f);
		DrawDebugSphere(World, CurrentLocation, DashHitRadius, 12, FColor::Cyan, false, 1.0f);
	}

	LastSweepLocation = CurrentLocation;

	UASCombatTargetSubsystem* CombatTargets = World->GetSubsystem<UASCombatTargetSubsystem>();
	if (!CombatTargets)
	{
		return;
	}

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Segment;
	Query.SearchOrigin = SweepStart;
	Query.EndLocation = CurrentLocation;
	Query.SearchRadius = DashHitRadius;
	Query.Instigator = Avatar;
	Query.bCanHitPlayers = bCanHitPlayers;
	Query.MaxTargets = 1;

	bool bHitTarget = false;
	if (HasSkillAuthority())
	{
		TArray<FGameplayEffectSpecHandle> EffectSpecs;
		const FGameplayEffectSpecHandle DamageSpec = MakeDamageSpec();
		if (DamageSpec.IsValid())
		{
			EffectSpecs.Add(DamageSpec);
		}

		bHitTarget = CombatTargets->ResolveAndApply(Query, EffectSpecs, &DashHitTargets).HasAnyHit();
	}
	else
	{
		Query.Domain = EASCombatTargetDomain::ActorOnly;
		bHitTarget = !CombatTargets->FindActorTargets(Query).IsEmpty();
	}

	if (!bHitTarget)
	{
		return;
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(World, CurrentLocation, DashHitRadius, 12, FColor::Red, false, 1.0f);
	}

	StopDash();
}

void UASSkillDash::StopDash()
{
	if (bDashStopped)
	{
		return;
	}
	bDashStopped = true;

	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		Avatar->GetWorldTimerManager().ClearTimer(DashHitTimerHandle);
	}

	//RootMotion 제거 (끝나면 속도 0으로 멈춤)
	if (DashTask)
	{
		DashTask->EndTask();
		DashTask = nullptr;
	}

	if (IsActive())
	{
		if (PostDashEffect)
		{
			FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(PostDashEffect);
			if (SpecHandle.IsValid())
			{
				if (PostDashDuration > 0.0f)
				{
					SpecHandle.Data->SetDuration(PostDashDuration, true);
				}
				
				FActiveGameplayEffectHandle Spec = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
			}
		}

		//착지 동작(End 섹션)이 있으면 재생 후 OnMontageCompleted에서 종료, 없으면 바로 종료
		if (!TryPlayEndSection())
		{
			FinishSkill(false);
		}
	}
}

bool UASSkillDash::TryPlayEndSection()
{
	if (!AnimMontage || DashEndSection.IsNone() || !AnimMontage->IsValidSectionName(DashEndSection))
	{
		return false;
	}

	//몽타주가 실제로 재생 중일 때만 (아니면 끝나는 신호가 안 와서 스킬이 안 끝남)
	const UAnimInstance* AnimInstance = GetActorInfo().GetAnimInstance();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!AnimInstance || !ASC || !AnimInstance->Montage_IsPlaying(AnimMontage))
	{
		return false;
	}

	//ASC로 점프해야 다른 플레이어 화면에도 복제됨
	ASC->CurrentMontageJumpToSection(DashEndSection);
	bPlayingEndSection = true;
	return true;
}

void UASSkillDash::OnMontageCompleted()
{
	//End 재생이 끝남 → 스킬 종료 (돌진 중에는 Loop이라 여기 안 옴, bEndAbilityManually라 부모는 무시)
	if (bPlayingEndSection)
	{
		bPlayingEndSection = false;
		FinishSkill(false);
		return;
	}

	Super::OnMontageCompleted();
}

void UASSkillDash::LockRotation()
{
	if (bRotationLocked)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	//원래 값 저장 후 끄기 (이동 방향·컨트롤러 방향으로 회전 안 함)
	bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
	bSavedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
	bRotationLocked = true;
}

void UASSkillDash::UnlockRotation()
{
	if (!bRotationLocked)
	{
		return;
	}
	bRotationLocked = false;

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
	}
}
