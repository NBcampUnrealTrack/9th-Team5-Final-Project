#include "Quests/Dialogue/ASQuestDialogueWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Items/UI/ASMenuStyle.h"
#include "Quests/Dialogue/ASQuestDialogueComponent.h"

void UASQuestDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree->RootWidget) return;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(ASMenuStyle::Background);
	Panel->SetPadding(FMargin(28.f, 24.f));
	UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Panel);
	CanvasSlot->SetAnchors(FAnchors(.16f, .64f, .84f, .95f));
	CanvasSlot->SetOffsets(FMargin(0.f));
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Content);
	SpeakerText = WidgetTree->ConstructWidget<UTextBlock>();
	ASMenuStyle::StyleText(SpeakerText, 17, ASMenuStyle::Accent, true);
	LineText = WidgetTree->ConstructWidget<UTextBlock>();
	LineText->SetAutoWrapText(true);
	LineText->SetLineHeightPercentage(1.35f);
	ASMenuStyle::StyleText(LineText, 18, ASMenuStyle::Text);
	Content->AddChildToVerticalBox(SpeakerText)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	Content->AddChildToVerticalBox(LineText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DialogueActions"));
	Content->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
	Actions->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UButton* Next = WidgetTree->ConstructWidget<UButton>();
	ASMenuStyle::StyleButton(Next, true);
	ButtonText = WidgetTree->ConstructWidget<UTextBlock>();
	ASMenuStyle::StyleText(ButtonText, 14, ASMenuStyle::Text, true);
	Next->SetContent(ButtonText);
	Next->OnClicked.AddDynamic(this, &ThisClass::Continue);
	UButton* Cancel = WidgetTree->ConstructWidget<UButton>();
	ASMenuStyle::StyleButton(Cancel);
	UTextBlock* CancelText = WidgetTree->ConstructWidget<UTextBlock>();
	CancelText->SetText(NSLOCTEXT("QuestDialogue", "End", "대화 종료"));
	ASMenuStyle::StyleText(CancelText, 14, ASMenuStyle::Muted);
	Cancel->SetContent(CancelText);
	Cancel->OnClicked.AddDynamic(this, &ThisClass::Close);
	Actions->AddChildToHorizontalBox(Cancel)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	Actions->AddChildToHorizontalBox(Next);
}

void UASQuestDialogueWidget::ShowLine(UASQuestDialogueComponent* InDialogue, const FText& Speaker, const FText& Line, const FText& ContinueLabel)
{
	Dialogue = InDialogue;
	if (SpeakerText) SpeakerText->SetText(Speaker);
	if (LineText) LineText->SetText(Line);
	if (ButtonText) ButtonText->SetText(ContinueLabel);
}

void UASQuestDialogueWidget::Continue()
{
	if (Dialogue.IsValid()) Dialogue->ServerAdvanceDialogue();
}

void UASQuestDialogueWidget::Close()
{
	if (Dialogue.IsValid()) Dialogue->ServerCloseDialogue();
}
