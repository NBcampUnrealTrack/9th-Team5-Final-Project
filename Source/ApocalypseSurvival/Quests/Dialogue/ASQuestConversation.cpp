#include "Quests/ASQuestComponent.h"
#include "Quests/Data/ASQuestDefinition.h"
#include "Quests/Dialogue/ASQuestDialogueComponent.h"

// Compatibility entry points delegate to the separate conversation component.
void UASQuestComponent::BeginNpcDialogue(AASQuestNpc* Npc)
{
	if (UASQuestDialogueComponent* Dialogue = GetOwner() ? GetOwner()->FindComponentByClass<UASQuestDialogueComponent>() : nullptr)
		Dialogue->BeginDialogue(Npc);
}

void UASQuestComponent::ServerAdvanceDialogue_Implementation()
{
	if (UASQuestDialogueComponent* Dialogue = GetOwner() ? GetOwner()->FindComponentByClass<UASQuestDialogueComponent>() : nullptr)
		Dialogue->ServerAdvanceDialogue();
}

void UASQuestComponent::ServerCloseDialogue_Implementation()
{
	if (UASQuestDialogueComponent* Dialogue = GetOwner() ? GetOwner()->FindComponentByClass<UASQuestDialogueComponent>() : nullptr)
		Dialogue->ServerCloseDialogue();
}

UASQuestDefinition* UASQuestComponent::FindQuestForNpc(FName NpcId) const
{
	if (!bPersistenceReady || NpcId.IsNone()) return nullptr;
	UASQuestDefinition* Best = nullptr;
	int32 BestRank = MAX_int32;
	for (UASQuestDefinition* Definition : QuestCatalog)
	{
		if (!Definition) continue;
		FASQuestProgress Progress;
		const bool bAccepted = GetQuestProgress(Definition->QuestId, Progress);
		int32 Rank;
		if (!bAccepted)
		{
			if (IsAutomaticQuest(Definition) || Definition->GetOfferNpcId() != NpcId || !CanAcceptQuest(Definition)) continue;
			Rank = 1;
		}
		else if (Progress.Status == EASQuestStatus::ReadyToTurnIn && Definition->TurnInNpcId == NpcId) Rank = 0;
		else if (Progress.Status == EASQuestStatus::InProgress
			&& (Definition->GetOfferNpcId() == NpcId || Definition->TurnInNpcId == NpcId)) Rank = 2;
		else continue;
		if (!Best || Rank < BestRank || (Rank == BestRank && Definition->DialoguePriority > Best->DialoguePriority))
		{
			Best = Definition;
			BestRank = Rank;
		}
	}
	return Best;
}
