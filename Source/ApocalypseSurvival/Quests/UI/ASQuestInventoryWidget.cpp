#include "Quests/UI/ASQuestInventoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Items/UI/ASMenuStyle.h"
#include "Items/UI/ASItemViewBinding.h"
#include "Quests/UI/ViewModels/ASQuestInventoryViewModel.h"

void UASQuestInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	WidgetTree->RootWidget = Scroll;
	Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Rows);
}

void UASQuestInventoryWidget::NativeConstruct()
{
	if (!ViewModel) ViewModel = NewObject<UASQuestInventoryViewModel>(this);
	ViewModel->Initialize(GetOwningPlayer());
	Super::NativeConstruct();
	ASItemViewBinding::Set(this, TEXT("QuestInventoryVM"), ViewModel);
	ViewModel->RemoveAllFieldValueChangedDelegates(this);
	ViewModel->AddFieldValueChangedDelegate(UASQuestInventoryViewModel::FFieldNotificationClassDescriptor::Presentation,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this, [this](UObject*, UE::FieldNotification::FFieldId) { Refresh(); }));
	Refresh();
}

void UASQuestInventoryWidget::NativeDestruct()
{
	if (ViewModel) { ViewModel->RemoveAllFieldValueChangedDelegates(this); ViewModel->Shutdown(); }
	Super::NativeDestruct();
}

void UASQuestInventoryWidget::AddLine(const FText& Text, FLinearColor Color)
{
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	Line->SetText(Text); Line->SetAutoWrapText(true);
	ASMenuStyle::StyleText(Line, 14, Color);
	Rows->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 8.f));
}

void UASQuestInventoryWidget::Refresh()
{
	if (!Rows) return;
	Rows->ClearChildren();
	auto Text = [&](const FText& Value, int32 Size, FLinearColor Color, bool bBold = false)
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
		Block->SetText(Value); Block->SetAutoWrapText(true);
		ASMenuStyle::StyleText(Block, Size, Color, bBold);
		return Block;
	};
	Rows->AddChildToVerticalBox(Text(NSLOCTEXT("QuestUI", "Journal", "임무 기록"), 24, ASMenuStyle::Text, true))->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
	if (!ViewModel) return;
	const FASQuestInventoryPresentation& Presentation = ViewModel->Presentation;
	if (!Presentation.EmptyMessage.IsEmpty()) AddLine(Presentation.EmptyMessage, ASMenuStyle::Muted);
	for (const FASQuestJournalEntryPresentation& Quest : Presentation.Entries)
	{
		UBorder* Rule = WidgetTree->ConstructWidget<UBorder>();
		Rule->SetBrushColor(ASMenuStyle::Line); Rule->SetPadding(FMargin(0.f, .5f));
		Rows->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
		Rows->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		Header->AddChildToHorizontalBox(Text(Quest.Title, 20, Quest.bReported ? ASMenuStyle::Muted : ASMenuStyle::Text, true))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Header->AddChildToHorizontalBox(Text(Quest.Status, 12, Quest.bReadyToReport ? ASMenuStyle::Accent : ASMenuStyle::Muted))->SetVerticalAlignment(VAlign_Center);
		if (Quest.bReported)
		{
			Rows->AddChildToVerticalBox(Text(NSLOCTEXT("QuestUI", "HandedIn", "의뢰인에게 보고하고 물품을 전달했습니다."), 13, ASMenuStyle::Muted))->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
			continue;
		}
		Rows->AddChildToVerticalBox(Text(Quest.Description, 14, ASMenuStyle::Muted))->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));
		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
		Rows->AddChildToVerticalBox(Columns)->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
		UVerticalBox* Goals = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* GoalSlot = Columns->AddChildToHorizontalBox(Goals);
		FSlateChildSize GoalSize(ESlateSizeRule::Fill); GoalSize.Value = .62f; GoalSlot->SetSize(GoalSize);
		GoalSlot->SetPadding(FMargin(0.f, 0.f, 28.f, 0.f));
		Goals->AddChildToVerticalBox(Text(NSLOCTEXT("QuestUI", "Objectives", "진행 목표"), 12, ASMenuStyle::Accent))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		for (const FASQuestGoalPresentation& Goal : Quest.Goals)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			Goals->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 7.f));
			const bool bDone = Goal.bComplete;
			USizeBox* MarkerSize = WidgetTree->ConstructWidget<USizeBox>(); MarkerSize->SetWidthOverride(6.f); MarkerSize->SetHeightOverride(6.f);
			UBorder* Marker = WidgetTree->ConstructWidget<UBorder>(); Marker->SetBrushColor(bDone ? ASMenuStyle::Success : ASMenuStyle::Line); MarkerSize->SetContent(Marker);
			UHorizontalBoxSlot* MarkerSlot = Row->AddChildToHorizontalBox(MarkerSize); MarkerSlot->SetVerticalAlignment(VAlign_Center); MarkerSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
			Row->AddChildToHorizontalBox(Text(Goal.Label, 14, bDone ? ASMenuStyle::Muted : ASMenuStyle::Text))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Row->AddChildToHorizontalBox(Text(Goal.Count, 13, bDone ? ASMenuStyle::Success : ASMenuStyle::Muted))->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}
		UBorder* ItemPanel = WidgetTree->ConstructWidget<UBorder>(); ItemPanel->SetBrushColor(ASMenuStyle::Surface); ItemPanel->SetPadding(FMargin(18.f));
		UHorizontalBoxSlot* ItemSlot = Columns->AddChildToHorizontalBox(ItemPanel); FSlateChildSize ItemSize(ESlateSizeRule::Fill); ItemSize.Value = .38f; ItemSlot->SetSize(ItemSize);
		UVerticalBox* Bag = WidgetTree->ConstructWidget<UVerticalBox>(); ItemPanel->SetContent(Bag);
		Bag->AddChildToVerticalBox(Text(NSLOCTEXT("QuestUI", "Evidence", "퀘스트 물품"), 12, ASMenuStyle::Accent))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		for (const FASQuestItemPresentation& Item : Quest.Items)
		{
			Bag->AddChildToVerticalBox(Text(Item.Name, 14, ASMenuStyle::Text))->SetPadding(FMargin(0.f, 6.f));
			Bag->AddChildToVerticalBox(Text(Item.Acquisition, 12, Item.bAcquired ? ASMenuStyle::Success : ASMenuStyle::Muted))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}
		if (Quest.Items.IsEmpty()) Bag->AddChildToVerticalBox(Text(NSLOCTEXT("QuestUI", "NoItems", "필요한 물품 없음"), 13, ASMenuStyle::Muted));
	}
}
