#include "ASSkillAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
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

	// 비용, 쿨다운
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bHasMontage = AnimMontage != nullptr;
	const bool bWaitForHitEvent = bHasMontage && HitEventTag.IsValid();

	bWaitingForHitEvent = bWaitForHitEvent;
	bHitEventReceived = false;

	// 몽타주보다 먼저 대기 시작
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

	// 몽타주 없음: 즉시 발동 후 종료
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
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASSkillAbility::OnSkillEffect_Implementation()
{
	if (!DamageEffectClass)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	const FVector Center = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * EffectForwardOffset;

	for (AActor* Target : FindTargetsInRadius(Center, EffectRadius))
	{
		ApplySkillDamage(Target);
	}
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

ACharacter* UASSkillAbility::GetOwnerCharacter() const
{
	// Owner는 PlayerState일 수 있음
	return Cast<ACharacter>(GetAvatarActorFromActorInfo());
}

TArray<AActor*> UASSkillAbility::FindTargetsInRadius(const FVector& Center, float Radius) const
{
	TArray<AActor*> Targets;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World || Radius <= 0.0f)
	{
		return Targets;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASSkillFindTargets), false, Avatar);

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
		if (!IsValid(Actor) || Actor == Avatar || Targets.Contains(Actor))
		{
			continue;
		}

		if (!bCanHitPlayers)
		{
			if (const APawn* Pawn = Cast<APawn>(Actor); Pawn && Pawn->IsPlayerControlled())
			{
				continue;
			}
		}

		if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor))
		{
			continue;
		}

		Targets.Add(Actor);
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(World, Center, Radius, 16, Targets.Num() > 0 ? FColor::Red : FColor::Green, false, 1.0f);
	}

	return Targets;
}

void UASSkillAbility::ApplySkillDamage(AActor* Target)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!Target || !DamageEffectClass || !ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// Health에 더하므로 음수
	SpecHandle.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -CalculateDamage());

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
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
