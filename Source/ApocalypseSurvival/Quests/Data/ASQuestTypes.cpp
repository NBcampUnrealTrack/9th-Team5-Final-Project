#include "Quests/Data/ASQuestTypes.h"
#include "Quests/Data/ASQuestDefinition.h"
#include "Quests/Data/ASQuestTags.h"
#include "GameFramework/PlayerState.h"
#include "Items/Data/ASItemDefinition.h"

void FASQuestObjectiveDefinition::UpgradeLegacyType()
{
	if (ObjectiveType != EASQuestObjectiveType::Custom) return;
	if (EventTag == ASGameplayTags::Quest_Event_ItemCollected && ItemDefinition)
		ObjectiveType = EASQuestObjectiveType::ItemCollected;
	else if (EventTag == ASGameplayTags::Quest_Event_EnemyKilled && !ItemDefinition && !TargetTag.IsValid())
		ObjectiveType = EASQuestObjectiveType::EnemyKilled;
	else if (EventTag == ASGameplayTags::Quest_Event_ZoneEntered && !ItemDefinition && !TargetMonsterDefinition)
		ObjectiveType = EASQuestObjectiveType::ZoneEntered;
	else if (EventTag == ASGameplayTags::Quest_Event_Interacted && !ItemDefinition && !TargetMonsterDefinition)
		ObjectiveType = EASQuestObjectiveType::Interacted;
}

FGameplayTag FASQuestObjectiveDefinition::GetEventTag() const
{
	switch (ObjectiveType)
	{
	case EASQuestObjectiveType::ItemCollected: return ASGameplayTags::Quest_Event_ItemCollected;
	case EASQuestObjectiveType::ZoneEntered: return ASGameplayTags::Quest_Event_ZoneEntered;
	case EASQuestObjectiveType::EnemyKilled: return ASGameplayTags::Quest_Event_EnemyKilled;
	case EASQuestObjectiveType::Interacted: return ASGameplayTags::Quest_Event_Interacted;
	default: return EventTag;
	}
}

FGameplayTag FASQuestObjectiveDefinition::GetTargetTag() const
{
	if (ObjectiveType == EASQuestObjectiveType::ItemCollected)
		return ItemDefinition ? ItemDefinition->QuestTargetTag : FGameplayTag();
	return ObjectiveType == EASQuestObjectiveType::EnemyKilled ? FGameplayTag() : TargetTag;
}

UASItemDefinition* FASQuestObjectiveDefinition::GetItemDefinition() const
{
	return ObjectiveType == EASQuestObjectiveType::ItemCollected || ObjectiveType == EASQuestObjectiveType::Custom
		? ItemDefinition.Get() : nullptr;
}

UASMonsterDefinition* FASQuestObjectiveDefinition::GetTargetMonsterDefinition() const
{
	return ObjectiveType == EASQuestObjectiveType::EnemyKilled || ObjectiveType == EASQuestObjectiveType::Custom
		? TargetMonsterDefinition.Get() : nullptr;
}

EASQuestKillCredit FASQuestObjectiveDefinition::GetKillCredit() const
{
	return ObjectiveType == EASQuestObjectiveType::EnemyKilled || ObjectiveType == EASQuestObjectiveType::Custom
		? KillCredit : EASQuestKillCredit::SharedRun;
}

bool FASQuestProgress::HasValidObjectives() const
{
	return IsValid(Definition) && Objectives.Num() == Definition->Objectives.Num();
}

bool FASQuestProgress::IsObjectiveComplete(int32 Index) const
{
	return HasValidObjectives() && Objectives.IsValidIndex(Index)
		&& Objectives[Index].CurrentCount >= Definition->Objectives[Index].RequiredCount;
}

bool FASQuestProgress::AreObjectivesComplete() const
{
	if (!HasValidObjectives()) return false;
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
		if (!IsObjectiveComplete(Index)) return false;
	return true;
}

TArray<int32> FASQuestProgress::GetEligibleObjectiveIndices() const
{
	TArray<int32> Result;
	if (Status != EASQuestStatus::InProgress || !HasValidObjectives()) return Result;
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		if (IsObjectiveComplete(Index)) continue;
		Result.Add(Index);
		if (Definition->bSequentialObjectives) break;
	}
	return Result;
}

bool ASQuestRules::MatchesObjective(const FASQuestObjectiveDefinition& Objective,
	const FASQuestEvent& Event, const UASItemDefinition* CollectedItem, const AActor* QuestOwner)
{
	return (!Objective.GetItemDefinition() || Objective.GetItemDefinition() == CollectedItem)
		&& Event.EventTag.MatchesTag(Objective.GetEventTag())
		&& (!Objective.GetTargetMonsterDefinition() || Event.MonsterDefinition == Objective.GetTargetMonsterDefinition())
		&& (Event.EventTag != ASGameplayTags::Quest_Event_EnemyKilled
			|| Objective.GetKillCredit() != EASQuestKillCredit::KillerOnly || Event.KillerPlayerState.Get() == QuestOwner)
		&& (!Objective.GetTargetTag().IsValid() || Event.TargetTag.MatchesTag(Objective.GetTargetTag()));
}
