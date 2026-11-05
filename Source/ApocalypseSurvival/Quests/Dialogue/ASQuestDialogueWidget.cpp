#include "Quests/Dialogue/ASQuestDialogueWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Quests/Dialogue/ASQuestDialogueComponent.h"

void UASQuestDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree->RootWidget) return;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(.025f, .035f, .05f, .97f));
	Panel->SetPadding(FMargin(24.f));
	UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Panel);
	CanvasSlot->SetAnchors(FAnchors(.15f, .58f, .85f, .95f));
	CanvasSlot->SetOffsets(FMargin(0.f));
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Content);
	SpeakerText = WidgetTree->ConstructWidget<UTextBlock>();
	LineText = WidgetTree->ConstructWidget<UTextBlock>();
	LineText->SetAutoWrapText(true);
	Content->AddChildToVerticalBox(SpeakerText);
	Content->AddChildToVerticalBox(LineText);
	UButton* Next = WidgetTree->ConstructWidget<UButton>();
	ButtonText = WidgetTree->ConstructWidget<UTextBlock>();
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Next->SetContent(ButtonText);
	Next->OnClicked.AddDynamic(this, &ThisClass::Continue);
	Content->AddChildToVerticalBox(Next);
	UButton* Cancel = WidgetTree->ConstructWidget<UButton>();
	UTextBlock* CancelText = WidgetTree->ConstructWidget<UTextBlock>();
	CancelText->SetText(FText::FromString(TEXT("닫기")));
	CancelText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Cancel->SetContent(CancelText);
	Cancel->OnClicked.AddDynamic(this, &ThisClass::Close);
	Content->AddChildToVerticalBox(Cancel);
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
