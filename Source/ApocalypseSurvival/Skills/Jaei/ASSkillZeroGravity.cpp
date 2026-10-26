#include "Skills/Jaei/ASSkillZeroGravity.h"

#include "GameplayEffect.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Jaei/ASZeroGravityAreaActor.h"

UASSkillZeroGravity::UASSkillZeroGravity()
{
	AreaClass = AASZeroGravityAreaActor::StaticClass();

	//커서 위치에 고정 장판
	TargetingType = EASSkillTargetingType::Location;
	bAttachToOwner = false;

	Radius = 400.0f;
	Duration = 6.0f;
	TickInterval = 0.5f;
}

void UASSkillZeroGravity::ConfigureArea(AASSkillAreaActor* Area)
{
	AASZeroGravityAreaActor* ZeroGravityArea = Cast<AASZeroGravityAreaActor>(Area);
	if (!ZeroGravityArea)
	{
		return;
	}

	const FGameplayEffectSpecHandle StunSpec = StunEffect
		? MakeOutgoingGameplayEffectSpec(StunEffect, GetAbilityLevel())
		: FGameplayEffectSpecHandle();

	//폭발 피해 = 1회 피해 x BurstDamageScale (모인 수 보너스는 장판이 끝날 때 곱함)
	FGameplayEffectSpecHandle BurstSpec = MakeDamageSpec();
	if (BurstSpec.IsValid())
	{
		BurstSpec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -CalculateDamage() * BurstDamageScale);
	}

	ZeroGravityArea->InitZeroGravity(
		StunSpec,
		StunChance.GetValueAtLevel(GetAbilityLevel()),
		StunRollCount,
		StunRollInterval,
		BurstSpec,
		BurstBonusPerTarget,
		BurstRadiusPerTarget,
		MaxBurstTargets,
		BurstImpactPreset);
}
