#include "Quests/Persistence/ASQuestSaveData.h"

bool FASQuestSaveData::Validate(FString& OutError) const
{
	OutError.Reset();
	if (DataVersion != CurrentVersion || Quests.Num() > ASQuestLimits::MaxQuests)
	{
		OutError = TEXT("Unsupported quest save version or too many saved quests (maximum 256).");
		return false;
	}
	TSet<FName> QuestIds;
	for (const FASQuestSaveRecord& Quest : Quests)
	{
		if (Quest.QuestId.IsNone() || QuestIds.Contains(Quest.QuestId) || Quest.Objectives.Num() > ASQuestLimits::MaxObjectives
			|| static_cast<uint8>(Quest.Status) > static_cast<uint8>(EASQuestStatus::RewardClaimed))
		{
			OutError = TEXT("Saved quests contain invalid IDs, duplicate IDs, statuses, or too many objectives.");
			return false;
		}
		QuestIds.Add(Quest.QuestId);
		TSet<FName> ObjectiveIds;
		for (const FASQuestObjectiveProgress& Objective : Quest.Objectives)
		{
			if (Objective.ObjectiveId.IsNone() || ObjectiveIds.Contains(Objective.ObjectiveId) || Objective.CurrentCount < 0)
			{
				OutError = TEXT("Saved objectives contain invalid IDs, duplicate IDs, or negative counts.");
				return false;
			}
			ObjectiveIds.Add(Objective.ObjectiveId);
		}
	}
	return true;
}
