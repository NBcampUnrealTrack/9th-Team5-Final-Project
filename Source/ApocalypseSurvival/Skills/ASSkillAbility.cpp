#include "ASSkillAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayEffect.h"
#include "Skills/ASSkillTags.h"

UASSkillAbility::UASSkillAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	HitEventTag = ASGameplayTags::Event_Skill_Hit;
}

void UASSkillAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//비용, 쿨다운
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bHasMontage = AnimMontage != nullptr;
	const bool bWaitForHitEvent = bHasMontage && HitEventTag.IsValid();

	bWaitingForHitEvent = bWaitForHitEvent;
	bHitEventReceived = false;

	//조준은 시전 순간에 한 번 확정
	CachedAimDirection = ComputeAimDirection();
	bHasCachedAim = true;

	if (bFaceAimDirection)
	{
		if (AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			Avatar->SetActorRotation(FRotator(0.0f, CachedAimDirection.Rotation().Yaw, 0.0f));
		}
	}

	//몽타주보다 먼저 대기 시작
	if (bWaitForHitEvent)
	{
		UAbilityTask_WaitGameplayEvent* WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, HitEventTag, nullptr, /*OnlyTriggerOnce*/ false, /*OnlyMatchExact*/ true);

		if (WaitHitTask)
		{
			WaitHitTask->EventReceived.AddDynamic(this, &UASSkillAbility::HandleHitEvent);
			WaitHitTask->ReadyForActivation();
		}
	}

	if (bHasMontage)
	{
		UAbilityTask_PlayMontageAndWait* PlayMontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this, NAME_None, AnimMontage, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds*/ true);

		if (PlayMontageTask)
		{
			PlayMontageTask->OnCompleted.AddDynamic(this, &UASSkillAbility::OnMontageCompleted);
			PlayMontageTask->OnBlendOut.AddDynamic(this, &UASSkillAbility::OnMontageCompleted);
			PlayMontageTask->OnInterrupted.AddDynamic(this, &UASSkillAbility::OnMontageInterrupted);
			PlayMontageTask->OnCancelled.AddDynamic(this, &UASSkillAbility::OnMontageInterrupted);
			PlayMontageTask->ReadyForActivation();

			if (!bWaitForHitEvent)
			{
				OnSkillEffect();
			}

			return;
		}
	}

	//몽타주 없음: 즉시 발동 후 종료
	OnSkillEffect();

	if (IsActive())
	{
		FinishSkill(false);
	}
}

void UASSkillAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility,
	bool bWasCancelled)
{
	bHasCachedAim = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASSkillAbility::OnSkillEffect_Implementation()
{
	//자식 클래스에서 구현
}

float UASSkillAbility::CalculateDamage_Implementation() const
{
	float AttackPower = 0.0f;

	if (AttackPowerAttribute.IsValid())
	{
		if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			bool bFound = false;
			AttackPower = ASC->GetGameplayAttributeValue(AttackPowerAttribute, bFound);
		}
	}

	return FMath::Max(0.0f, BaseDamage + AttackPower * AttackPowerScale);
}

FVector UASSkillAbility::GetAimDirection_Implementation() const
{
	return bHasCachedAim ? CachedAimDirection : ComputeAimDirection();
}

FVector UASSkillAbility::ComputeAimDirection() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return FVector::ForwardVector;
	}

	//TODO: 임시 커서 조준. 서버는 클라이언트의 커서를 모르므로 멀티에서는 TargetData로 교체 필요
	if (bAimAtCursor)
	{
		const APawn* Pawn = Cast<APawn>(Avatar);
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

		FHitResult Hit;
		if (PC && PC->IsLocalController() && PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			const FVector ToCursor = (Hit.ImpactPoint - Avatar->GetActorLocation()).GetSafeNormal2D();
			if (!ToCursor.IsNearlyZero())
			{
				return ToCursor;
			}
		}
	}

	return Avatar->GetActorForwardVector().GetSafeNormal2D();
}

ACharacter* UASSkillAbility::GetOwnerCharacter() const
{
	//Owner는 PlayerState일 수 있음
	return Cast<ACharacter>(GetAvatarActorFromActorInfo());
}

bool UASSkillAbility::HasSkillAuthority() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	return ActorInfo && ActorInfo->IsNetAuthority();
}

bool UASSkillAbility::IsValidSkillTarget(const AActor* Target, const AActor* Instigator, bool bCanHitPlayers)
{
	if (!IsValid(Target) || Target == Instigator)
	{
		return false;
	}

	if (!bCanHitPlayers)
	{
		if (const APawn* Pawn = Cast<APawn>(Target); Pawn && Pawn->IsPlayerControlled())
		{
			return false;
		}
	}

	// 투사체와 범위 공격 모두 이 함수를 거치므로, 여기서 같은 팀/중립 팀을 한 번에 제외한다.
	// 공격자는 적대 팀에만 피해를 준다. 따라서 몬스터 투사체는 플레이어만, 플레이어 공격은
	// 몬스터만 유효 타깃으로 판단한다.
	if (const IGenericTeamAgentInterface* InstigatorTeam = Cast<IGenericTeamAgentInterface>(Instigator))
	{
		if (InstigatorTeam->GetTeamAttitudeTowards(*Target) != ETeamAttitude::Hostile)
		{
			return false;
		}
	}

	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target)) != nullptr;
}

TArray<AActor*> UASSkillAbility::FindSkillTargets(const UWorld* World, const FVector& Center, float Radius, const AActor* Instigator, bool bCanHitPlayers)
{
	TArray<AActor*> Targets;

	if (!World || Radius <= 0.0f)
	{
		return Targets;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASSkillFindTargets), false, Instigator);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (IsValidSkillTarget(Actor, Instigator, bCanHitPlayers))
		{
			Targets.AddUnique(Actor);
		}
	}

	return Targets;
}

void UASSkillAbility::ApplyDamageSpecToTarget(const FGameplayEffectSpecHandle& DamageSpec, AActor* Target)
{
	if (!DamageSpec.IsValid() || !Target)
	{
		return;
	}

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
	}
}

TArray<AActor*> UASSkillAbility::FindTargetsInRadius(const FVector& Center, float Radius) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;

	TArray<AActor*> Targets = FindSkillTargets(World, Center, Radius, Avatar, bCanHitPlayers);

	if (bDrawDebug && World)
	{
		DrawDebugSphere(World, Center, Radius, 16, Targets.Num() > 0 ? FColor::Red : FColor::Green, false, 1.0f);
	}

	return Targets;
}

FGameplayEffectSpecHandle UASSkillAbility::MakeDamageSpec() const
{
	if (!DamageEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
	if (SpecHandle.IsValid())
	{
		//Health에 더하므로 음수
		SpecHandle.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -CalculateDamage());
	}

	return SpecHandle;
}

void UASSkillAbility::ApplySkillDamage(AActor* Target)
{
	if (!HasSkillAuthority())
	{
		return;
	}

	ApplyDamageSpecToTarget(MakeDamageSpec(), Target);
}

void UASSkillAbility::HandleHitEvent(FGameplayEventData Payload)
{
	bHitEventReceived = true;
	OnSkillEffect();
}

void UASSkillAbility::OnMontageCompleted()
{
	FinishSkill(false);
}

void UASSkillAbility::OnMontageInterrupted()
{
	FinishSkill(true);
}

void UASSkillAbility::FinishSkill(bool bWasCancelled)
{
#if !UE_BUILD_SHIPPING
	if (bWaitingForHitEvent && !bHitEventReceived && !bWasCancelled)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Skill] %s: %s에 '%s' AnimNotify가 없습니다."),
			*GetName(), *GetNameSafe(AnimMontage), *HitEventTag.ToString());
	}
#endif

	bWaitingForHitEvent = false;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}
