#include "ASSkillBuff.h"

#include "GameplayEffect.h"
#include "Skills/Data/ASSkillData.h"

UASSkillBuff::UASSkillBuff()
{
	bShowRangeIndicator = false;
	bAimAtCursor = false;
	bFaceAimDirection = false;
	TargetingType = EASSkillTargetingType::Self;
}

void UASSkillBuff::OnSkillEffect_Implementation()
{
	for (const TSubclassOf<UGameplayEffect>& EffectClass : BuffEffects)
	{
		ApplyBuffEffect(EffectClass);
	}
}

void UASSkillBuff::ApplyBuffEffect(TSubclassOf<UGameplayEffect> EffectClass)
{
	if (!EffectClass)
	{
		return;
	}
	
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(EffectClass);
	
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	if (BuffDuration > 0.0f)
	{
		SpecHandle.Data->SetDuration(BuffDuration, true);
	}
	
	ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
}

void UASSkillBuff::ApplySkillStats(const FASSkillStatsRow& Stats)
{
	Super::ApplySkillStats(Stats);
	
	BuffDuration = Stats.BuffDuration;
}
