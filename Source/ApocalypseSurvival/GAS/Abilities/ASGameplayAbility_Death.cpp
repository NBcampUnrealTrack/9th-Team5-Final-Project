#include "GAS/Abilities/ASGameplayAbility_Death.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/ASAbilityTags.h"
#include "TimerManager.h"

UASGameplayAbility_Death::UASGameplayAbility_Death()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 서버가 시작하고 본인 클라이언트에도 활성화를 전달한다. 클라이언트는 몽타주를 자기 화면에서 재생한다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// 입력 태그가 아니라 사망 이벤트로 켜진다.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ASGameplayTags::GameplayEvent_Death;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);

	// 죽음 자체가 "죽어도 살아남는" 어빌리티다. 아래 취소 단계에서 자기 자신을 지우지 않게 한다.
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(ASGameplayTags::Ability_Behavior_SurvivesDeath);
	SetAssetTags(AssetTags);
}

void UASGameplayAbility_Death::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();

	// 회피나 스킬 도중 죽으면 끊는다. SurvivesDeath 가 붙은 것과 자기 자신은 남긴다.
	FGameplayTagContainer SurvivingTags;
	SurvivingTags.AddTag(ASGameplayTags::Ability_Behavior_SurvivesDeath);
	AbilitySystemComponent->CancelAbilities(nullptr, &SurvivingTags, this);

	// 죽음은 중간에 취소되지 않는다.
	SetCanBeCanceled(false);

	// 서버에서만 실제로 태그가 붙는다. 클라이언트는 복제된 태그로 같은 연출을 한다.
	if (AASPlayerCharacter* Character = GetPlayerCharacter())
	{
		Character->StartDeath();
	}

	// BP 자식의 ActivateAbility 이벤트가 있으면 여기서 불린다.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 멀티에서 쓰러져 있다가 죽으면 이미 누운 자세라 사망 몽타주를 건너뛴다.
	// (BleedOut 은 사망 요청 뒤에 쓰러짐 태그를 떼므로, 이 시점에는 아직 쓰러진 상태다)
	// 쓰러진 자세로 죽으면(멀티) 누운 채 그대로 둔다.
	// 쓰러질 때 미리 복제된 값이라 서버와 클라이언트 판정이 같다.
	const AASPlayerCharacter* DeathCharacter = GetPlayerCharacter();
	const bool bAlreadyLyingDown = DeathCharacter && DeathCharacter->IsLyingDown();

	if (DeathMontage && !bAlreadyLyingDown)
	{
		// 끝나도 몽타주를 멈추지 않는다. 쓰러진 자세를 리스폰 때까지 유지해야 한다.
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, DeathMontage, 1.0f, NAME_None, /*bStopWhenAbilityEnds*/ false);

		MontageTask->OnCompleted.AddDynamic(this, &UASGameplayAbility_Death::HandleMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &UASGameplayAbility_Death::HandleMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &UASGameplayAbility_Death::HandleMontageFinished);
		MontageTask->OnCancelled.AddDynamic(this, &UASGameplayAbility_Death::HandleMontageFinished);
		MontageTask->ReadyForActivation();

		// 마지막 자세 유지(Auto Blend Out 끔)면 몽타주 종료 알림이 오지 않으므로,
		// 몽타주 길이만큼 지나면 사망 처리를 마저 진행한다.
		if (bAutoFinishDeath)
		{
			GetWorld()->GetTimerManager().SetTimer(MinDurationTimerHandle, this, &UASGameplayAbility_Death::FinishDeath,
				FMath::Max(DeathMontage->GetPlayLength(), MinDeathDuration), false);
		}
	}
	else if (bAutoFinishDeath)
	{
		// 몽타주가 없거나 쓰러진 채 죽으면 최소 시간만 기다린다. 0 이면 바로 끝낸다.
		if (MinDeathDuration > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(MinDurationTimerHandle, this, &UASGameplayAbility_Death::FinishDeath, MinDeathDuration, false);
		}
		else
		{
			FinishDeath();
		}
	}
}

void UASGameplayAbility_Death::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility,
	bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MinDurationTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASGameplayAbility_Death::FinishDeath()
{
	if (!IsActive())
	{
		return;
	}

	// 리스폰 요청은 서버만 한다. 클라이언트 쪽 인스턴스는 어빌리티만 닫는다.
	if (GetCurrentActorInfo()->IsNetAuthority())
	{
		if (AASPlayerCharacter* Character = GetPlayerCharacter())
		{
			Character->FinishDeath();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UASGameplayAbility_Death::HandleMontageFinished()
{
	if (bAutoFinishDeath)
	{
		FinishDeath();
	}
}

AASPlayerCharacter* UASGameplayAbility_Death::GetPlayerCharacter() const
{
	return Cast<AASPlayerCharacter>(GetAvatarActorFromActorInfo());
}
