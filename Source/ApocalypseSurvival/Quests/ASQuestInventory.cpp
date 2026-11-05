#include "Quests/ASQuestComponent.h"
#include "Quests/Data/ASQuestDefinition.h"
#include "Quests/Data/ASQuestTags.h"
#include "Items/Data/ASItemDefinition.h"

int32 UASQuestComponent::GetCollectibleQuestItemCount(const UASItemDefinition* Item) const
{
	if (!bPersistenceReady || !Item || Item->ItemType != EASItemType::Quest) return 0;
	FASQuestEvent Event;
	Event.EventTag = ASGameplayTags::Quest_Event_ItemCollected;
	Event.TargetTag = Item->QuestTargetTag;
	int32 Count = 0;
	for (const FASQuestProgress& Quest : Quests)
	{
		for (int32 Index : Quest.GetEligibleObjectiveIndices())
		{
			const FASQuestObjectiveDefinition& Objective = Quest.Definition->Objectives[Index];
			const int32 Remaining = FMath::Max(0, Objective.RequiredCount - Quest.Objectives[Index].CurrentCount);
			if (Objective.GetItemDefinition() == Item && ASQuestRules::MatchesObjective(Objective, Event, Item, GetOwner()))
				Count = FMath::Max(Count, Remaining);
		}
	}
	return Count;
}

int32 UASQuestComponent::CollectQuestItem(UASItemDefinition* Item, int32 Quantity)
{
	if (!HasQuestAuthority() || Quantity <= 0) return 0;
	const int32 Collected = FMath::Min(Quantity, GetCollectibleQuestItemCount(Item));
	if (Collected <= 0) return 0;
	FASQuestEvent Event;
	Event.EventTag = ASGameplayTags::Quest_Event_ItemCollected;
	Event.TargetTag = Item->QuestTargetTag;
	Event.Amount = Collected;
	return ApplyQuestEvent(Event, Item) ? Collected : 0;
}

TArray<FASQuestItemEntry> UASQuestComponent::GetQuestItems(bool bIncludeMissing) const
{
	TArray<FASQuestItemEntry> Entries;
	for (const FASQuestProgress& Quest : Quests)
	{
		if (!Quest.Definition) continue;
		const auto Add = [&](UASItemDefinition* Item, int32 Count, int32 Required)
		{
			if (!Item) return;
			if (Quest.Status == EASQuestStatus::RewardClaimed) Count = 0;
			if (Count <= 0 && !bIncludeMissing) return;
			FASQuestItemEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.QuestId = Quest.QuestId;
			Entry.ItemDefinition = Item;
			Entry.Quantity = Count;
			Entry.RequiredCount = Required;
			Entry.Status = Quest.Status;
		};
		for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
		{
			if (!Quest.Definition->Objectives.IsValidIndex(Index)) break;
			const auto& Objective = Quest.Definition->Objectives[Index];
			Add(Objective.GetItemDefinition(), Quest.Objectives[Index].CurrentCount, Objective.RequiredCount);
			for (const FASItemGrant& Grant : Objective.CompletionItems)
				Add(Grant.ItemDefinition, Quest.IsObjectiveComplete(Index) ? Grant.Quantity : 0, Grant.Quantity);
		}
		for (const FASItemGrant& Grant : Quest.Definition->BossClearItems)
			Add(Grant.ItemDefinition, Quest.Status == EASQuestStatus::ReadyToTurnIn ? Grant.Quantity : 0, Grant.Quantity);
	}
	return Entries;
}
