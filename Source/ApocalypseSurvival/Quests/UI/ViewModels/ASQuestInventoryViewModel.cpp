#include "Quests/UI/ViewModels/ASQuestInventoryViewModel.h"
#include "Core/Framework/ASPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/Data/ASItemDefinition.h"
#include "Quests/ASQuestComponent.h"
#include "Quests/Data/ASQuestDefinition.h"
#include "Quests/Data/ASQuestTags.h"

bool FASQuestJournalEntryPresentation::operator==(const FASQuestJournalEntryPresentation& Other) const
{
	return QuestId == Other.QuestId && Title.EqualTo(Other.Title) && Description.EqualTo(Other.Description) && Status.EqualTo(Other.Status)
		&& bReported == Other.bReported && bReadyToReport == Other.bReadyToReport && Goals == Other.Goals && Items == Other.Items;
}

void UASQuestInventoryViewModel::Initialize(APlayerController* Controller)
{
	Shutdown();
	Player = Controller;
	ObservedController = Cast<AASPlayerController>(Controller);
	if (ObservedController.IsValid()) ObservedController->OnLocalPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	Refresh();
}

void UASQuestInventoryViewModel::Shutdown()
{
	if (Quests.IsValid()) Quests->OnQuestsChanged.RemoveDynamic(this, &ThisClass::Refresh);
	if (ObservedController.IsValid()) ObservedController->OnLocalPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	Quests.Reset(); ObservedController.Reset(); Player.Reset();
}

void UASQuestInventoryViewModel::BeginDestroy() { Shutdown(); Super::BeginDestroy(); }
void UASQuestInventoryViewModel::HandlePawnChanged(APawn* Pawn) { Refresh(); }

void UASQuestInventoryViewModel::Refresh()
{
	APlayerState* State = Player.IsValid() ? Player->PlayerState.Get() : nullptr;
	UASQuestComponent* Source = State ? State->FindComponentByClass<UASQuestComponent>() : nullptr;
	if (Quests.Get() != Source)
	{
		if (Quests.IsValid()) Quests->OnQuestsChanged.RemoveDynamic(this, &ThisClass::Refresh);
		Quests = Source;
		if (Quests.IsValid()) Quests->OnQuestsChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	}
	FASQuestInventoryPresentation Value;
	Value.bLoading = !Source || !Source->IsPersistenceReady();
	if (Value.bLoading)
	{
		Value.EmptyMessage = NSLOCTEXT("QuestUI", "Loading", "기록을 불러오는 중입니다.");
		UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
		return;
	}
	const auto Items = Source->GetQuestItems(true);
	for (const FASQuestProgress& Quest : Source->GetQuests())
	{
		if (!Quest.Definition) continue;
		FASQuestJournalEntryPresentation& Entry = Value.Entries.AddDefaulted_GetRef();
		Entry.QuestId = Quest.QuestId; Entry.Title = Quest.Definition->DisplayName; Entry.Description = Quest.Definition->Description;
		Entry.bReported = Quest.Status == EASQuestStatus::RewardClaimed;
		Entry.bReadyToReport = Quest.Status == EASQuestStatus::ReadyToTurnIn;
		Entry.Status = Entry.bReported ? NSLOCTEXT("QuestUI", "Reported", "보고 완료")
			: Entry.bReadyToReport ? NSLOCTEXT("QuestUI", "Ready", "보고 가능")
			: Quest.AreObjectivesComplete() ? NSLOCTEXT("QuestUI", "AwaitingBoss", "생환 대기") : NSLOCTEXT("QuestUI", "Active", "진행 중");
		if (Entry.bReported) continue;
		auto AddGoal = [&](const FText& Label, int32 Current, int32 Required)
		{
			FASQuestGoalPresentation& Goal = Entry.Goals.AddDefaulted_GetRef();
			Goal.Label = Label; Goal.bComplete = Current >= Required;
			Goal.Count = FText::Format(NSLOCTEXT("QuestUI", "Count", "{0} / {1}"), FText::AsNumber(Current), FText::AsNumber(Required));
		};
		for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
			if (Quest.Definition->Objectives.IsValidIndex(Index)) AddGoal(Quest.Definition->Objectives[Index].Description, Quest.Objectives[Index].CurrentCount, Quest.Definition->Objectives[Index].RequiredCount);
		const bool bHasBossGoal = Quest.Definition->Objectives.ContainsByPredicate([](const FASQuestObjectiveDefinition& Goal) { return Goal.GetEventTag() == ASGameplayTags::Quest_Event_FinalBossDefeated; });
		if (!bHasBossGoal) AddGoal(NSLOCTEXT("QuestUI", "ReturnAlive", "최종보스 처치 후 생환"), Entry.bReadyToReport ? 1 : 0, 1);
		for (const FASQuestItemEntry& Item : Items)
		{
			if (Item.QuestId != Quest.QuestId || !Item.ItemDefinition) continue;
			FASQuestItemPresentation& Evidence = Entry.Items.AddDefaulted_GetRef();
			Evidence.Name = Item.ItemDefinition->DisplayName; Evidence.bAcquired = Item.Quantity > 0;
			Evidence.Acquisition = FText::Format(NSLOCTEXT("QuestUI", "ItemCount", "{0} / {1}  ·  {2}"), FText::AsNumber(Item.Quantity), FText::AsNumber(Item.RequiredCount),
				Evidence.bAcquired ? NSLOCTEXT("QuestUI", "Acquired", "확보") : NSLOCTEXT("QuestUI", "Missing", "미확보"));
		}
	}
	if (Value.Entries.IsEmpty()) Value.EmptyMessage = NSLOCTEXT("QuestUI", "Empty", "진행 중인 의뢰가 없습니다.");
	UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
}
