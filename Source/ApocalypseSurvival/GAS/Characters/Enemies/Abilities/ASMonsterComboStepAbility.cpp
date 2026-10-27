#include "GAS/Characters/Enemies/Abilities/ASMonsterComboStepAbility.h"

#include "AbilitySystemBlueprintLibrary.h"

bool UASMonsterComboStepAbility::ExecutePatternEffect()
{
	OnSkillEffect();
	FinishComboStep(false);
	// FinishComboStep이 Ability를 종료하므로 부모의 FinishPattern은 실행하지 않는다.
	return false;
}

void UASMonsterComboStepAbility::FinishComboStep(const bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!bWasCancelled && Avatar && CompletionEventTag.IsValid())
	{
		FGameplayEventData EventData;
		EventData.EventTag = CompletionEventTag;
		EventData.Instigator = Avatar;
		EventData.Target = Avatar;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, CompletionEventTag, EventData);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}
