#include "Characters/Enemy/Data/ASMonsterStatDefinition.h"

#include "Algo/Sort.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMonsterStatDefinition)

namespace ASMonsterStatTags
{
	UE_DEFINE_GAMEPLAY_TAG(MaxHealth, "Stat.Monster.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(MoveSpeed, "Stat.Monster.MoveSpeed");
	UE_DEFINE_GAMEPLAY_TAG(AttackPower, "Stat.Monster.AttackPower");
}

UASMonsterStatDefinition::UASMonsterStatDefinition()
{
	// 새 DataAsset을 만들었을 때 바로 세 핵심 전투 스탯을 볼 수 있게 기본 행을 제공한다.
	// 기존 MonsterDefinition은 StatDefinition을 지정하기 전까지 이 값의 영향을 받지 않는다.
	FASMonsterStatEntry& MaxHealth = Stats.AddDefaulted_GetRef();
	MaxHealth.StatTag = ASMonsterStatTags::MaxHealth;
	MaxHealth.BaseValue = 100.0f;
	MaxHealth.MinValue = 1.0f;
	MaxHealth.MaxValue = 1000000.0f;
	MaxHealth.QuantizationStep = 1.0f;

	FASMonsterStatEntry& MoveSpeed = Stats.AddDefaulted_GetRef();
	MoveSpeed.StatTag = ASMonsterStatTags::MoveSpeed;
	MoveSpeed.BaseValue = 300.0f;
	MoveSpeed.MinValue = 0.0f;
	MoveSpeed.MaxValue = 10000.0f;
	MoveSpeed.QuantizationStep = 0.1f;

	FASMonsterStatEntry& AttackPower = Stats.AddDefaulted_GetRef();
	AttackPower.StatTag = ASMonsterStatTags::AttackPower;
	AttackPower.BaseValue = 10.0f;
	AttackPower.MinValue = 0.0f;
	AttackPower.MaxValue = 1000000.0f;
	AttackPower.QuantizationStep = 0.01f;
}

float FASResolvedMonsterStats::GetValue(const FGameplayTag& StatTag, const float FallbackValue) const
{
	if (const float* Value = Values.Find(StatTag))
	{
		return *Value;
	}
	return FallbackValue;
}

void FASResolvedMonsterStats::SetValue(const FGameplayTag& StatTag, const float Value)
{
	if (StatTag.IsValid())
	{
		Values.FindOrAdd(StatTag) = Value;
	}
}

const FASMonsterStatEntry* UASMonsterStatDefinition::FindStat(const FGameplayTag& StatTag) const
{
	return Stats.FindByPredicate([&StatTag](const FASMonsterStatEntry& Entry)
	{
		return Entry.StatTag == StatTag;
	});
}

const FASMonsterStatGrowthRule* UASMonsterStatDefinition::FindGrowthRule(const FGameplayTag& StatTag) const
{
	return GrowthRules.FindByPredicate([&StatTag](const FASMonsterStatGrowthRule& Rule)
	{
		return Rule.StatTag == StatTag;
	});
}

FASResolvedMonsterStats FASMonsterStatResolver::Resolve(const UASMonsterStatDefinition& Definition,
	const FASMonsterStatResolveContext& Context,
	TConstArrayView<FASMonsterStatModifier> Modifiers)
{
	FASResolvedMonsterStats Result;
	for (const FASMonsterStatEntry& Entry : Definition.Stats)
	{
		if (!Entry.StatTag.IsValid())
		{
			continue;
		}

		Result.SetValue(Entry.StatTag, ResolveValue(Entry, Definition.FindGrowthRule(Entry.StatTag), Context, Modifiers));
	}
	return Result;
}

float FASMonsterStatResolver::ResolveValue(const FASMonsterStatEntry& Entry,
	const FASMonsterStatGrowthRule* GrowthRule,
	const FASMonsterStatResolveContext& Context,
	TConstArrayView<FASMonsterStatModifier> Modifiers)
{
	float RoundMultiplier = 1.0f;
	float DifficultyMultiplier = 1.0f;
	if (GrowthRule)
	{
		if (GrowthRule->RoundMultiplierCurve)
		{
			RoundMultiplier = GrowthRule->RoundMultiplierCurve->GetFloatValue(FMath::Max(1, Context.Round));
		}
		if (GrowthRule->DifficultyMultiplierCurve)
		{
			DifficultyMultiplier = GrowthRule->DifficultyMultiplierCurve->GetFloatValue(FMath::Max(0.0f, Context.Difficulty));
		}
	}

	float PreFlat = 0.0f;
	float PercentAdd = 0.0f;
	float PostFlat = 0.0f;
	TArray<FASMonsterStatModifier> PercentMultModifiers;
	const FASMonsterStatModifier* WinningOverride = nullptr;

	for (const FASMonsterStatModifier& Modifier : Modifiers)
	{
		if (Modifier.StatTag != Entry.StatTag)
		{
			continue;
		}

		switch (Modifier.Operation)
		{
		case EASMonsterStatModifierOperation::PreFlat:
			PreFlat += Modifier.Magnitude;
			break;
		case EASMonsterStatModifierOperation::PercentAdd:
			PercentAdd += Modifier.Magnitude;
			break;
		case EASMonsterStatModifierOperation::PercentMult:
			PercentMultModifiers.Add(Modifier);
			break;
		case EASMonsterStatModifierOperation::PostFlat:
			PostFlat += Modifier.Magnitude;
			break;
		case EASMonsterStatModifierOperation::Override:
			if (!WinningOverride
				|| Modifier.ChannelPriority > WinningOverride->ChannelPriority
				|| (Modifier.ChannelPriority == WinningOverride->ChannelPriority && Modifier.Priority > WinningOverride->Priority))
			{
				WinningOverride = &Modifier;
			}
			break;
		default:
			break;
		}
	}

	// 독립 배율은 Channel/Priority 순으로 고정해 Actor GAS와 Mass가 같은 입력에서 같은 결과를 낸다.
	PercentMultModifiers.Sort([](const FASMonsterStatModifier& A, const FASMonsterStatModifier& B)
	{
		return A.ChannelPriority == B.ChannelPriority ? A.Priority < B.Priority : A.ChannelPriority < B.ChannelPriority;
	});

	float Value = (Entry.BaseValue * RoundMultiplier * DifficultyMultiplier + PreFlat) * (1.0f + PercentAdd);
	for (const FASMonsterStatModifier& Modifier : PercentMultModifiers)
	{
		Value *= Modifier.Magnitude;
	}
	Value += PostFlat;

	if (WinningOverride)
	{
		Value = WinningOverride->Magnitude;
	}

	const float MinValue = FMath::Min(Entry.MinValue, Entry.MaxValue);
	const float MaxValue = FMath::Max(Entry.MinValue, Entry.MaxValue);
	Value = FMath::Clamp(Value, MinValue, MaxValue);
	if (Entry.QuantizationStep > UE_SMALL_NUMBER)
	{
		Value = FMath::GridSnap(Value, Entry.QuantizationStep);
	}
	return Value;
}
