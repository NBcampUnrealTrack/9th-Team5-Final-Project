#include "Quests/UI/ASQuestInventoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/Data/ASItemDefinition.h"
#include "Quests/ASQuestComponent.h"
#include "Quests/Data/ASQuestDefinition.h"
#include "Quests/Data/ASQuestTags.h"

void UASQuestInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Bounds = WidgetTree->ConstructWidget<USizeBox>();
	Bounds->SetMinDesiredHeight(440.f);
	Bounds->SetMinDesiredWidth(620.f);
	WidgetTree->RootWidget = Bounds;
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Bounds->SetContent(Scroll);
	Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Rows);
}

void UASQuestInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	APlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
	Quests = State ? State->FindComponentByClass<UASQuestComponent>() : nullptr;
	if (Quests.IsValid()) Quests->OnQuestsChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	Refresh();
}

void UASQuestInventoryWidget::NativeDestruct()
{
	if (Quests.IsValid()) Quests->OnQuestsChanged.RemoveDynamic(this, &ThisClass::Refresh);
	Quests.Reset();
	Super::NativeDestruct();
}

void UASQuestInventoryWidget::AddLine(const FText& Text, FLinearColor Color)
{
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	Line->SetText(Text); Line->SetColorAndOpacity(FSlateColor(Color)); Line->SetAutoWrapText(true);
	Rows->AddChildToVerticalBox(Line)->SetPadding(FMargin(12.f, 7.f));
}

void UASQuestInventoryWidget::Refresh()
{
	if (!Rows) return;
	Rows->ClearChildren();
	AddLine(FText::FromString(TEXT("퀘스트 보관함")), FLinearColor(.4f, .8f, 1.f));
	if (!Quests.IsValid() || !Quests->IsPersistenceReady())
	{
		AddLine(FText::FromString(TEXT("퀘스트 정보를 불러오는 중입니다."))); return;
	}
	const auto Items = Quests->GetQuestItems(true);
	const auto Progress = Quests->GetQuests();
	if (Progress.IsEmpty()) AddLine(FText::FromString(TEXT("수락한 퀘스트가 없습니다.")));
	for (const FASQuestProgress& Quest : Progress)
	{
		if (!Quest.Definition) continue;
		const bool bObjectivesComplete = Quest.AreObjectivesComplete();
		const TCHAR* Status = Quest.Status == EASQuestStatus::RewardClaimed ? TEXT("완료")
			: Quest.Status == EASQuestStatus::ReadyToTurnIn ? TEXT("생환 · 완료 가능")
			: bObjectivesComplete ? TEXT("최종보스 처치 대기") : TEXT("진행 중");
		AddLine(FText::Format(FText::FromString(TEXT("{0}  —  {1}")), Quest.Definition->DisplayName, FText::FromString(Status)), FLinearColor(.4f,.8f,1.f));
		for (const FASQuestItemEntry& Item : Items)
		{
			if (Item.QuestId != Quest.QuestId || !Item.ItemDefinition) continue;
			const TCHAR* Acquisition = Item.Status == EASQuestStatus::RewardClaimed ? TEXT("제출 완료")
				: Item.Quantity == 0 ? TEXT("미획득") : Item.Status == EASQuestStatus::ReadyToTurnIn ? TEXT("보관 중")
				: Quest.Definition->bRunBoundObjectives ? TEXT("획득 · 이번 판 목표 완료 필요") : TEXT("획득");
			AddLine(FText::Format(FText::FromString(TEXT("{0}   {1}/{2}   [{3}]")), Item.ItemDefinition->DisplayName,
				FText::AsNumber(Item.Quantity), FText::AsNumber(Item.RequiredCount), FText::FromString(Acquisition)),
				Item.Quantity > 0 ? FLinearColor(.5f,1.f,.65f) : FLinearColor(.65f,.7f,.75f));
		}
		for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
		{
			if (!Quest.Definition->Objectives.IsValidIndex(Index)) break;
			const auto& Objective = Quest.Definition->Objectives[Index];
			AddLine(FText::Format(FText::FromString(TEXT("{0}  {1}/{2}")), Objective.Description,
				FText::AsNumber(Quest.Objectives[Index].CurrentCount), FText::AsNumber(Objective.RequiredCount)));
		}
		const bool bHasLegacyBossObjective = Quest.Definition->Objectives.ContainsByPredicate(
			[](const FASQuestObjectiveDefinition& Objective) { return Objective.GetEventTag() == ASGameplayTags::Quest_Event_FinalBossDefeated; });
		if (!bHasLegacyBossObjective)
			AddLine(FText::Format(FText::FromString(TEXT("선행 목표 달성 후 최종보스 처치  {0}/1")),
				FText::AsNumber(Quest.Status == EASQuestStatus::InProgress ? 0 : 1)));
	}
}
