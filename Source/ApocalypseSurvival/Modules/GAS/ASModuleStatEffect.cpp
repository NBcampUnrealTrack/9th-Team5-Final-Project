#include "Modules/GAS/ASModuleStatEffect.h"

#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "NativeGameplayTags.h"

namespace
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(AttackPowerTag, "Data.Module.Stat.AttackPower");
UE_DEFINE_GAMEPLAY_TAG_STATIC(MaxHealthTag, "Data.Module.Stat.MaxHealth");
UE_DEFINE_GAMEPLAY_TAG_STATIC(MaxManaTag, "Data.Module.Stat.MaxMana");
UE_DEFINE_GAMEPLAY_TAG_STATIC(MaxStaminaTag, "Data.Module.Stat.MaxStamina");
UE_DEFINE_GAMEPLAY_TAG_STATIC(MoveSpeedTag, "Data.Module.Stat.MoveSpeed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(CoreValueTag, "Data.Module.Core.Value");

FGameplayTag GetStatTag(EASModuleStat Stat)
{
	switch (Stat)
	{
	case EASModuleStat::AttackPower: return AttackPowerTag;
	case EASModuleStat::MaxHealth: return MaxHealthTag;
	case EASModuleStat::MaxMana: return MaxManaTag;
	case EASModuleStat::MaxStamina: return MaxStaminaTag;
	case EASModuleStat::MoveSpeed: return MoveSpeedTag;
	default: return FGameplayTag();
	}
}
}

UASModuleStatEffect::UASModuleStatEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	const auto AddStat = [this](const FGameplayAttribute& Attribute, EASModuleStat Stat)
	{
		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		FSetByCallerFloat Magnitude;
		Magnitude.DataTag = GetStatTag(Stat);
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	};
	AddStat(UASCombatAttributeSet::GetAttackPowerAttribute(), EASModuleStat::AttackPower);
	AddStat(UASHealthAttributeSet::GetMaxHealthAttribute(), EASModuleStat::MaxHealth);
	AddStat(UASPlayerAttributeSet::GetMaxManaAttribute(), EASModuleStat::MaxMana);
	AddStat(UASPlayerAttributeSet::GetMaxStaminaAttribute(), EASModuleStat::MaxStamina);
	AddStat(UASCombatAttributeSet::GetMoveSpeedAttribute(), EASModuleStat::MoveSpeed);
}

void UASModuleStatEffect::SetMagnitude(FGameplayEffectSpec& Spec, EASModuleStat Stat, float Value)
{
	for (const FGameplayModifierInfo& Modifier : GetDefault<UASModuleStatEffect>()->Modifiers)
	{
		Spec.SetSetByCallerMagnitude(Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag, 0.0f);
	}
	Spec.SetSetByCallerMagnitude(GetStatTag(Stat), Value);
}

FGameplayTag UASModuleStatEffect::GetCoreMagnitudeTag()
{
	return CoreValueTag;
}
