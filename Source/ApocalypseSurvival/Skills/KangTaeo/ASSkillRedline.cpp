#include "Skills/KangTaeo/ASSkillRedline.h"

#include "GameplayEffect.h"
#include "Skills/ASSkillTags.h"

UASSkillRedline::UASSkillRedline()
{
	//시전자를 따라다니는 충격파 장판
	bAttachToOwner = true;
	bDamageOnSpawn = true;
	Radius = 500.0f;
	Duration = 8.0f;
	TickInterval = 1.0f;

	//궁극기는 누르면 바로 시전
	bShowRangeIndicator = false;
	bAimAtCursor = false;
	TargetingType = EASSkillTargetingType::Self;

	RedlineStateTag = ASGameplayTags::State_Taeo_Redline;
}

void UASSkillRedline::OnSkillEffect_Implementation()
{
	//자기 버프는 서버·클라이언트 모두 적용 (LocalPredicted라 내 화면에서 바로 반영)
	for (const TSubclassOf<UGameplayEffect>& EffectClass : SelfEffects)
	{
		ApplySelfEffect(EffectClass);
	}

	//충격파 장판 생성 (서버에서만, 부모가 처리)
	Super::OnSkillEffect_Implementation();
}

void UASSkillRedline::ApplySelfEffect(TSubclassOf<UGameplayEffect> EffectClass)
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

	//버프 시간 = 장판 시간 (SkillData가 있으면 DT의 Area Duration 값)
	if (Duration > 0.0f)
	{
		SpecHandle.Data->SetDuration(Duration, true);
	}

	//GE가 걸려 있는 동안 레드라인 상태 태그 부여
	if (RedlineStateTag.IsValid())
	{
		SpecHandle.Data->DynamicGrantedTags.AddTag(RedlineStateTag);
	}

	ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
}
