#include "GAS/Characters/Enemies/Abilities/ASMonsterComboAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

UASMonsterComboAbility::UASMonsterComboAbility()
{
	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack);
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_Combo);
	SetAssetTags(AssetTagsToSet);
	// 같은 보스에서 두 Combo GA가 동시에 이벤트를 기다리지 않게 한다.
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_Attacking);
}

void UASMonsterComboAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!HasSkillAuthority() || ComboSteps.IsEmpty() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishCombo(true);
		return;
	}

	ActiveStepIndex = INDEX_NONE;
	ActiveStepAbilityTag = FGameplayTag();
	StartNextStep();
}

void UASMonsterComboAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	StopWaitingForStep();
	CancelActiveStep();
	ActiveStepIndex = INDEX_NONE;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterComboAbility::StartNextStep()
{
	if (!IsActive())
	{
		return;
	}

	++ActiveStepIndex;
	if (!ComboSteps.IsValidIndex(ActiveStepIndex))
	{
		FinishCombo(false);
		return;
	}

	const FASMonsterComboStep& Step = ComboSteps[ActiveStepIndex];
	if (!Step.AbilityTag.IsValid() || !Step.CompletionEventTag.IsValid())
	{
		FinishCombo(true);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		FinishCombo(true);
		return;
	}

	ActiveStepAbilityTag = Step.AbilityTag;
	BeginWaitingForStep(Step);
	if (!CompletionEventTask)
	{
		FinishCombo(true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		FinishCombo(true);
		return;
	}

	Avatar->GetWorldTimerManager().SetTimer(
		StepTimeoutTimerHandle, this, &ThisClass::OnStepTimedOut, FMath::Max(0.01f, Step.Timeout), false);

	FGameplayTagContainer ActiveAbilityTags;
	ActiveAbilityTags.AddTag(ActiveStepAbilityTag);
	if (!ASC->TryActivateAbilitiesByTag(ActiveAbilityTags, false))
	{
		FinishCombo(true);
	}
}

void UASMonsterComboAbility::BeginWaitingForStep(const FASMonsterComboStep& Step)
{
	StopWaitingForStep();

	CompletionEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, Step.CompletionEventTag, nullptr, /*OnlyTriggerOnce*/ true, /*OnlyMatchExact*/ true);
	if (CompletionEventTask)
	{
		CompletionEventTask->EventReceived.AddDynamic(this, &ThisClass::OnStepCompleted);
		CompletionEventTask->ReadyForActivation();
	}
}

void UASMonsterComboAbility::StopWaitingForStep()
{
	if (CompletionEventTask)
	{
		CompletionEventTask->EndTask();
		CompletionEventTask = nullptr;
	}

	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		Avatar->GetWorldTimerManager().ClearTimer(StepTimeoutTimerHandle);
		Avatar->GetWorldTimerManager().ClearTimer(TransitionTimerHandle);
	}
}

void UASMonsterComboAbility::CancelActiveStep()
{
	if (!ActiveStepAbilityTag.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayTagContainer ActiveAbilityTags;
		ActiveAbilityTags.AddTag(ActiveStepAbilityTag);
		ASC->CancelAbilities(&ActiveAbilityTags, nullptr, nullptr);
	}

	ActiveStepAbilityTag = FGameplayTag();
}

void UASMonsterComboAbility::OnStepCompleted(FGameplayEventData EventData)
{
	if (!IsActive() || !ComboSteps.IsValidIndex(ActiveStepIndex))
	{
		return;
	}

	const FASMonsterComboStep& FinishedStep = ComboSteps[ActiveStepIndex];
	if (EventData.EventTag != FinishedStep.CompletionEventTag)
	{
		return;
	}

	if (EventData.Instigator != GetAvatarActorFromActorInfo())
	{
		return;
	}

	StopWaitingForStep();
	// 완료 이벤트는 하위 Ability가 EndAbility를 호출하기 직전에 발생한다.
	// 다음 틱 이후에 시작해 이전 단계의 태그·타이머 정리가 끝난 것을 보장한다.
	ActiveStepAbilityTag = FGameplayTag();
	const float Delay = FMath::Max(KINDA_SMALL_NUMBER, FinishedStep.TransitionDelay);
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		Avatar->GetWorldTimerManager().SetTimer(
			TransitionTimerHandle, this, &ThisClass::StartNextStep, Delay, false);
	}
	else
	{
		FinishCombo(true);
	}
}

void UASMonsterComboAbility::OnStepTimedOut()
{
	FinishCombo(true);
}

void UASMonsterComboAbility::FinishCombo(const bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}
