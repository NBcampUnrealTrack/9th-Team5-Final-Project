#include "Core/UI/ASSessionInviteWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"

UASSessionInviteWidget::UASSessionInviteWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UASSessionInviteWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0, 0, 0, 0.8f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(480);
	Background->SetContent(Size);
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.06f, 0.08f, 0.12f));
	Panel->SetPadding(FMargin(24));
	Size->SetContent(Panel);
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Layout);
	Message = WidgetTree->ConstructWidget<UTextBlock>();
	Message->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(Message)->SetPadding(FMargin(0, 0, 0, 16));
	PasswordInput = WidgetTree->ConstructWidget<UEditableTextBox>();
	PasswordInput->SetIsPassword(true);
	PasswordInput->SetHintText(FText::FromString(TEXT("방 비밀번호 (숫자 4자리)")));
	Layout->AddChildToVerticalBox(PasswordInput)->SetPadding(FMargin(0, 0, 0, 16));
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Buttons);
	auto AddButton = [this, Buttons](const TCHAR* Text)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		UHorizontalBoxSlot* ButtonSlot = Buttons->AddChildToHorizontalBox(Button);
		ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ButtonSlot->SetPadding(FMargin(4));
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Label->SetMargin(FMargin(12));
		Button->SetContent(Label);
		return Button;
	};
	AddButton(TEXT("참가"))->OnClicked.AddUniqueDynamic(this, &ThisClass::Confirm);
	AddButton(TEXT("취소"))->OnClicked.AddUniqueDynamic(this, &ThisClass::Cancel);
}

void UASSessionInviteWidget::Configure(const FString& RoomName, const bool bRequiresPassword)
{
	bPasswordRequired = bRequiresPassword;
	Message->SetText(FText::FromString(FString::Printf(TEXT("Steam 친구 초대: %s\n%s"), *RoomName,
		bRequiresPassword ? TEXT("비밀번호를 입력한 후 참가해 주세요.") : TEXT("이 방에 참가하시겠습니까?"))));
	PasswordInput->SetVisibility(bRequiresPassword ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UASSessionInviteWidget::Confirm()
{
	const FString Password = PasswordInput->GetText().ToString();
	if (bPasswordRequired && !UASSessionSubsystem::IsValidRoomPassword(Password))
	{
		Message->SetText(FText::FromString(TEXT("비밀번호는 숫자 4자리로 입력해 주세요.")));
		PasswordInput->SetKeyboardFocus();
		return;
	}
	if (UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>();
		Sessions && Sessions->AcceptSessionInvite(Password)) RemoveFromParent();
}

void UASSessionInviteWidget::Cancel()
{
	if (UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>()) Sessions->DismissSessionInvite();
	RemoveFromParent();
}

FReply UASSessionInviteWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!InKeyEvent.IsRepeat()) Cancel();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		if (!InKeyEvent.IsRepeat()) Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
