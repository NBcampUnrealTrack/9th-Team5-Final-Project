#include "Quests/Data/ASQuestDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UASQuestDefinition::ValidateDefinition(FString& OutError) const
{
	OutError.Reset();
	if (QuestId.IsNone() || Objectives.IsEmpty() || Objectives.Num() > ASQuestLimits::MaxObjectives || ExperienceReward < 0)
	{
		OutError = TEXT("QuestId and 1-64 objectives are required; ExperienceReward cannot be negative.");
		return false;
	}

	TSet<FName> ObjectiveIds;
	for (const FASQuestObjectiveDefinition& Objective : Objectives)
	{
		if (Objective.ObjectiveId.IsNone() || ObjectiveIds.Contains(Objective.ObjectiveId)
			|| !Objective.EventTag.IsValid() || Objective.RequiredCount <= 0)
		{
			OutError = TEXT("Objectives need unique, non-empty IDs, a valid EventTag, and a positive RequiredCount.");
			return false;
		}
		ObjectiveIds.Add(Objective.ObjectiveId);
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UASQuestDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateDefinition(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
