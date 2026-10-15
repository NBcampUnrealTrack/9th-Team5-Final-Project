#include "ASCharacterDefinition.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Misc/DataValidation.h"

float UASCharacterDefinition::GetBaseAttributeValue(const FGameplayAttribute& Attribute, float DefaultValue) const
{
	for (const FASAttributeInitEntry& Entry : BaseAttributes)
	{
		if (Entry.Attribute == Attribute)
		{
			return Entry.Value;
		}
	}
	return DefaultValue;
}

float UASCharacterDefinition::GetGrowthPerLevel(const FGameplayAttribute& Attribute) const
{
	for (const FASAttributeGrowEntry& Entry : LevelGrowth)
	{
		if (Entry.Attribute == Attribute)
		{
			return Entry.ValuePerLevel;
		}
	}
	return 0.f;
}

float UASCharacterDefinition::GetAttributeValueAtLevel(const FGameplayAttribute& Attribute, int32 Level, float DefaultValue) const
{
	const int32 LevelsGained = FMath::Max(0, Level - 1);
	return GetBaseAttributeValue(Attribute, DefaultValue) + GetGrowthPerLevel(Attribute) * LevelsGained;
}

#if WITH_EDITOR
EDataValidationResult UASCharacterDefinition::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	
	if (!CharacterClass)
	{
		Context.AddError(FText::FromString(TEXT("CharacterClass가 비어 있음")));
		Result = EDataValidationResult::Invalid;
	}
	
	// 기본값: 빈 칸, 중복 확인
	TSet<FGameplayAttribute> SeenBase;
	for (const FASAttributeInitEntry& Entry : BaseAttributes)
	{
		if (!Entry.Attribute.IsValid())
		{
			Context.AddError(FText::FromString(TEXT("BaseAttribute에 어트리뷰트가 비어 있는 항목이 있음")));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		if (SeenBase.Contains(Entry.Attribute))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("BaseAttributes에 %s 중복"), *Entry.Attribute.GetName())));
			Result = EDataValidationResult::Invalid;
		}
		SeenBase.Add(Entry.Attribute);
	}
	
	// 성장: 빈 칸, 중복 확인
	TSet<FGameplayAttribute> SeenGrowth;
	for (const FASAttributeGrowEntry& Entry : LevelGrowth)
	{
		if (!Entry.Attribute.IsValid())
		{
			Context.AddError(FText::FromString(TEXT("LevelGrowth에 어트리뷰트가 비어 있는 항목이 있음")));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		if (SeenGrowth.Contains(Entry.Attribute))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("LevelGrowth에 %s 중복"), *Entry.Attribute.GetName())));
			Result = EDataValidationResult::Invalid;
		}
		SeenGrowth.Add(Entry.Attribute);
	}
	
	return Result;
}
#endif