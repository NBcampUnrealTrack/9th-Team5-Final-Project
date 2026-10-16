#include "ASProgressionData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ASProgressionData"

int32 UASProgressionData::GetExpToNextLevel(int32 Level) const
{
	if (Level >= MaxLevel || ExpToNextLevel.Num() == 0)
	{
		return 0;
	}
	
	const int32 Index = FMath::Clamp(Level - 1, 0, ExpToNextLevel.Num() - 1);
	return FMath::Max(1, ExpToNextLevel[Index]);
}

int32 UASProgressionData::FindSkillUpgradeTierIndex(int32 Level) const
{
	return SkillUpgradeTiers.IndexOfByPredicate([Level](const FASSkillUpgradeTier& Tier)
	{
		return Tier.Level == Level;
	});
}

int32 UASProgressionData::GetUnlockedSkillUpgradeTierCount(int32 Level) const
{
	int32 Count = 0;
	for (const FASSkillUpgradeTier& Tier : SkillUpgradeTiers)
	{
		if (Tier.Level <= Level)
		{
			++Count;
		}
	}
	return Count;
}

#if WITH_EDITOR
EDataValidationResult UASProgressionData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	
	int32 PrevLevel = 1;
	for (int32 Index = 0; Index < SkillUpgradeTiers.Num(); ++Index)
	{
		const FASSkillUpgradeTier& Tier = SkillUpgradeTiers[Index];
		
		if (Tier.Level <= PrevLevel)
		{
			Context.AddError(FText::Format(LOCTEXT("TierOrder", "SkillUpgradeTiers[{0}] 레벨 {1}: 레벨 오름차순이어야 하고 중복될 수 없다."), FText::AsNumber(Index), FText::AsNumber(Tier.Level)));
			Result = EDataValidationResult::Invalid;
		}
		
		if (Tier.Level > MaxLevel)
		{
			Context.AddError(FText::Format(LOCTEXT("TierOverMax", "SkillUpgradeTiers[{0}] 레벨 {1}: 최대 레벨({2})보다 높다."), FText::AsNumber(Index), FText::AsNumber(Tier.Level), FText::AsNumber(MaxLevel)));
			Result = EDataValidationResult::Invalid;
		}
		
		if (!Tier.SkillSlotTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("TierNoSlot", "SkillUpgradeTiers[{0}]: 스킬 슬롯이 비어 있다."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
		
		PrevLevel = FMath::Max(PrevLevel, Tier.Level);
	}
	
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE