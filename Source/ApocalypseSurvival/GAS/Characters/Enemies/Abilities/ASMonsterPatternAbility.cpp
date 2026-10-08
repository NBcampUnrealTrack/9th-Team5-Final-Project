#include "GAS/Characters/Enemies/Abilities/ASMonsterPatternAbility.h"

#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UASMonsterPatternAbility::UASMonsterPatternAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = true;
	AbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_Attacking);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_Dead);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_HitReact);
}

void UASMonsterPatternAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!HasSkillAuthority() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BeginPatternWindup();
	if (WindupDuration <= 0.0f)
	{
		BeginPatternEffect();
		return;
	}

	ACharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character) || !Character->GetWorld())
	{
		FinishPattern(true);
		return;
	}

	Character->GetWorldTimerManager().SetTimer(WindupTimerHandle, this,
		&UASMonsterPatternAbility::BeginPatternEffect, WindupDuration, false);
}

void UASMonsterPatternAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(WindupTimerHandle);
	}
	EndPattern(bWasCancelled);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterPatternAbility::BeginPatternWindup()
{
}

bool UASMonsterPatternAbility::ExecutePatternEffect()
{
	OnSkillEffect();
	return true;
}

void UASMonsterPatternAbility::EndPattern(bool bWasCancelled)
{
}

void UASMonsterPatternAbility::FinishPattern(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UASMonsterPatternAbility::BeginPatternEffect()
{
	if (IsActive() && ExecutePatternEffect())
	{
		FinishPattern(false);
	}
}
