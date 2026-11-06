#include "Quests/Data/ASQuestCatalog.h"
#include "Quests/Data/ASQuestDefinition.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UASQuestCatalog::ValidateDefinitions(const TArray<TObjectPtr<UASQuestDefinition>>& Definitions, FString& OutError)
{
	OutError.Reset();
	if (Definitions.Num() > ASQuestLimits::MaxQuests)
	{
		OutError = TEXT("A quest catalog may contain at most 256 quests.");
		return false;
	}
	TMap<FName, const UASQuestDefinition*> ById;
	for (const UASQuestDefinition* Quest : Definitions)
	{
		if (!IsValid(Quest) || !Quest->ValidateDefinition(OutError))
		{
			OutError = FString::Printf(TEXT("Invalid quest %s: %s"), *GetNameSafe(Quest), *OutError);
			return false;
		}
		if (ById.Contains(Quest->QuestId))
		{
			OutError = FString::Printf(TEXT("Duplicate QuestId in catalog: %s."), *Quest->QuestId.ToString());
			return false;
		}
		ById.Add(Quest->QuestId, Quest);
	}
	TSet<FName> Visiting, Visited;
	TFunction<bool(const UASQuestDefinition*)> Visit = [&](const UASQuestDefinition* Quest)
	{
		if (Visited.Contains(Quest->QuestId)) return true;
		if (Visiting.Contains(Quest->QuestId))
		{
			OutError = FString::Printf(TEXT("Quest prerequisite cycle includes %s."), *Quest->QuestId.ToString());
			return false;
		}
		Visiting.Add(Quest->QuestId);
		for (FName Id : Quest->PrerequisiteQuestIds)
		{
			const UASQuestDefinition* const* Required = ById.Find(Id);
			if (!Required)
			{
				OutError = FString::Printf(TEXT("Quest %s requires unregistered quest %s."), *Quest->QuestId.ToString(), *Id.ToString());
				return false;
			}
			if (!Visit(*Required)) return false;
		}
		Visiting.Remove(Quest->QuestId);
		Visited.Add(Quest->QuestId);
		return true;
	};
	for (const UASQuestDefinition* Quest : Definitions) if (!Visit(Quest)) return false;
	return true;
}

#if WITH_EDITOR
EDataValidationResult UASQuestCatalog::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateCatalog(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
