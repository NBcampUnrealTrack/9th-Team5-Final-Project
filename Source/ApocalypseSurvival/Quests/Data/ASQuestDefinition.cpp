#include "Quests/Data/ASQuestDefinition.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Loot/ASLootValidation.h"
#include "Quests/Data/ASQuestTags.h"
#include "UObject/ObjectSaveContext.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UASQuestDefinition::PostLoad()
{
	Super::PostLoad();
	if (ObjectiveDataVersion == 0)
	{
		for (FASQuestObjectiveDefinition& Objective : Objectives) Objective.UpgradeLegacyType();
		ObjectiveDataVersion = 1;
	}
}

void UASQuestDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	// A newly authored Custom goal must remain Custom when the asset is loaded again.
	ObjectiveDataVersion = 1;
	Super::PreSave(SaveContext);
}

bool UASQuestDefinition::ValidateDefinition(FString& OutError) const
{
	OutError.Reset();
	if (QuestId.IsNone() || Objectives.Num() > ASQuestLimits::MaxObjectives || ExperienceReward < 0)
	{
		OutError = TEXT("QuestId is required; at most 64 objectives are allowed and ExperienceReward cannot be negative. Final boss clear is always required.");
		return false;
	}

	TSet<FName> ObjectiveIds;
	TSet<FName> Prerequisites;
	for (FName Id : PrerequisiteQuestIds)
	{
		if (Id.IsNone() || Id == QuestId || Prerequisites.Contains(Id))
		{
			OutError = TEXT("Prerequisites must be unique, non-empty, and cannot reference this quest.");
			return false;
		}
		Prerequisites.Add(Id);
	}
	if (((!bAutoAccept || !PrerequisiteQuestIds.IsEmpty()) && !GetOfferNpcId().IsNone() && (OfferDialogue.IsEmpty() || ProgressDialogue.IsEmpty()))
		|| (!TurnInNpcId.IsNone() && (ProgressDialogue.IsEmpty() || CompletionDialogue.IsEmpty())))
	{
		OutError = TEXT("Offer NPCs require offer/progress dialogue; turn-in NPCs require progress/completion dialogue.");
		return false;
	}
	for (const FASQuestObjectiveDefinition& Objective : Objectives)
	{
		const FGameplayTag EventTag = Objective.GetEventTag();
		const FGameplayTag TargetTag = Objective.GetTargetTag();
		const UASItemDefinition* Item = Objective.GetItemDefinition();
		const bool bMonsterKill = EventTag == ASGameplayTags::Quest_Event_EnemyKilled;
		if (static_cast<uint8>(Objective.ObjectiveType) > static_cast<uint8>(EASQuestObjectiveType::Interacted)
			|| (Objective.ObjectiveType == EASQuestObjectiveType::ItemCollected && !Item))
		{
			OutError = TEXT("Choose a valid objective type; Collect Item requires an ItemDefinition.");
			return false;
		}
		if ((Objective.GetTargetMonsterDefinition() && !bMonsterKill)
			|| (bMonsterKill && TargetTag.IsValid())
			|| static_cast<uint8>(Objective.GetKillCredit()) > static_cast<uint8>(EASQuestKillCredit::KillerOnly))
		{
			OutError = TEXT("Monster objectives use EnemyKilled, TargetMonsterDefinition and KillCredit. Leave TargetTag empty.");
			return false;
		}
		if (!FASLootValidation::ValidateGrants(Objective.CompletionItems, OutError)) return false;
		for (const FASItemGrant& Grant : Objective.CompletionItems)
		{
			if (Grant.ItemDefinition->ItemType != EASItemType::Quest)
			{
				OutError = TEXT("Objective completion items must be quest items.");
				return false;
			}
		}
		if (Item && (Item->ItemType != EASItemType::Quest || Item->ItemId.IsNone()
			|| EventTag != ASGameplayTags::Quest_Event_ItemCollected
			|| !TargetTag.IsValid() || !Item->QuestTargetTag.MatchesTag(TargetTag)))
		{
			OutError = TEXT("Item objectives require a quest item with a valid ID and matching quest target tag.");
			return false;
		}
		if (Objective.ObjectiveId.IsNone() || ObjectiveIds.Contains(Objective.ObjectiveId)
			|| !EventTag.IsValid() || Objective.RequiredCount <= 0)
		{
			OutError = TEXT("Objectives need unique, non-empty IDs, a valid EventTag, and a positive RequiredCount.");
			return false;
		}
		ObjectiveIds.Add(Objective.ObjectiveId);
	}
	if (!FASLootValidation::ValidateGrants(BossClearItems, OutError)) return false;
	for (const FASItemGrant& Item : BossClearItems)
	{
		if (Item.ItemDefinition->ItemType != EASItemType::Quest)
		{
			OutError = TEXT("Completion items must be quest items.");
			return false;
		}
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
