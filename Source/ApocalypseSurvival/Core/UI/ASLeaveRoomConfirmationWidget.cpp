#include "Core/UI/ASLeaveRoomConfirmationWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"

UASLeaveRoomConfirmationWidget::UASLeaveRoomConfirmationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UASLeaveRoomConfirmationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;
	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
	PanelSize->SetWidthOverride(460.0f);
	Background->SetContent(PanelSize);
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
	Panel->SetPadding(FMargin(24.0f));
	PanelSize->SetContent(Panel);
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Layout);
	UTextBlock* Message = WidgetTree->ConstructWidget<UTextBlock>();
	Message->SetText(FText::FromString(TEXT("메인메뉴로 돌아가시겠습니까?")));
	Message->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(Message)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Buttons);
	auto AddButton = [this, Buttons](const TCHAR* Text)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetHeightOverride(48.0f);
		UHorizontalBoxSlot* Slot = Buttons->AddChildToHorizontalBox(Size);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetPadding(FMargin(4.0f, 0.0f));
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		Size->SetContent(Button);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Button->SetContent(Label);
		return Button;
	};
	AddButton(TEXT("확인"))->OnClicked.AddUniqueDynamic(this, &ThisClass::Confirm);
	AddButton(TEXT("취소"))->OnClicked.AddUniqueDynamic(this, &ThisClass::Cancel);
}

void UASLeaveRoomConfirmationWidget::NativeDestruct()
{
	if (UUserWidget* Parent = ReturnFocusWidget.Get()) Parent->SetIsEnabled(true);
	OnConfirmed.Unbind();
	Super::NativeDestruct();
}

FReply UASLeaveRoomConfirmationWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!InKeyEvent.IsRepeat()) Cancel();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UASLeaveRoomConfirmationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

void UASLeaveRoomConfirmationWidget::Confirm()
{
	if (bResolved) return;
	bResolved = true;
	FSimpleDelegate Action = OnConfirmed;
	RemoveFromParent();
	Action.ExecuteIfBound();
}

void UASLeaveRoomConfirmationWidget::Cancel()
{
	if (bResolved) return;
	bResolved = true;
	RemoveFromParent();
	if (UUserWidget* Parent = ReturnFocusWidget.Get(); Parent && Parent->IsInViewport() && Parent->IsFocusable())
	{
		Parent->SetKeyboardFocus();
	}
}
