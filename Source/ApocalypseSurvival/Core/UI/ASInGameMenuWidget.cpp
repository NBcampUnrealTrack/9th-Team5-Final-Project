#include "Core/UI/ASInGameMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Core/Framework/ASPlayerController.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Core/UI/ASSettingsWidget.h"
#include "Core/UI/ASLeaveRoomConfirmationWidget.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"

UASInGameMenuWidget::UASInGameMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UASInGameMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree->RootWidget) return;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	WidgetTree->RootWidget = Background;
	MenuScreenSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("MenuScreenSwitcher"));
	Background->SetContent(MenuScreenSwitcher);
	UBorder* MenuPanel = WidgetTree->ConstructWidget<UBorder>();
	MenuPanel->SetBrushColor(FLinearColor::Transparent);
	MenuPanel->SetHorizontalAlignment(HAlign_Center);
	MenuPanel->SetVerticalAlignment(VAlign_Center);
	MenuScreenSwitcher->AddChild(MenuPanel);
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	MenuPanel->SetContent(Layout);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(FText::FromString(TEXT("메뉴")));
	Title->SetJustification(ETextJustify::Center);
	Layout->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	auto AddButton = [this, Layout](const FName Name, const TCHAR* Text)
	{
		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>();
		ButtonSize->SetWidthOverride(360.0f);
		ButtonSize->SetHeightOverride(50.0f);
		Layout->AddChildToVerticalBox(ButtonSize)->SetPadding(FMargin(0.0f, 4.0f));
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		ButtonSize->SetContent(Button);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 20;
		Label->SetFont(Font);
		Button->SetContent(Label);
		return Button;
	};
	ResumeButton = AddButton(TEXT("ResumeButton"), TEXT("계속하기 (ESC)"));
	SettingsButton = AddButton(TEXT("SettingsButton"), TEXT("환경설정"));
	LeaveRoomButton = AddButton(TEXT("LeaveRoomButton"), TEXT("방 나가기"));
}

void UASInGameMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ResumeButton) ResumeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ResumeGame);
	if (SettingsButton) SettingsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenSettings);
	if (LeaveRoomButton) LeaveRoomButton->OnClicked.AddUniqueDynamic(this, &ThisClass::LeaveRoom);
}

void UASInGameMenuWidget::NativeDestruct()
{
	if (LeaveConfirmation) LeaveConfirmation->RemoveFromParent();
	if (ResumeButton) ResumeButton->OnClicked.RemoveDynamic(this, &ThisClass::ResumeGame);
	if (SettingsButton) SettingsButton->OnClicked.RemoveDynamic(this, &ThisClass::OpenSettings);
	if (LeaveRoomButton) LeaveRoomButton->OnClicked.RemoveDynamic(this, &ThisClass::LeaveRoom);
	if (SettingsPanel)
	{
		SettingsPanel->OnBackRequested.Unbind();
		SettingsPanel->CloseKeyBindings();
	}
	Super::NativeDestruct();
}

FReply UASInGameMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!InKeyEvent.IsRepeat() && !bLeavingRoom)
		{
			if (SettingsPanel && MenuScreenSwitcher && MenuScreenSwitcher->GetActiveWidget() == SettingsPanel)
			{
				SettingsPanel->RefreshSettings();
				ShowMainMenu();
			}
			else ResumeGame();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UASInGameMenuWidget::ResumeGame()
{
	if (bLeavingRoom) return;
	if (AASPlayerController* Player = GetOwningPlayer<AASPlayerController>()) Player->CloseInGameMenu();
}

void UASInGameMenuWidget::OpenSettings()
{
	AASPlayerController* Player = GetOwningPlayer<AASPlayerController>();
	if (bLeavingRoom || !Player || !MenuScreenSwitcher) return;
	if (!SettingsPanel)
	{
		if (!Player->GetInGameSettingsWidgetClass())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Menu] Set InGameSettingsWidgetClass on the player controller."));
			return;
		}
		SettingsPanel = CreateWidget<UASSettingsWidget>(Player, Player->GetInGameSettingsWidgetClass());
		if (!SettingsPanel) return;
		SettingsPanel->OnBackRequested.BindUObject(this, &ThisClass::ShowMainMenu);
		MenuScreenSwitcher->AddChild(SettingsPanel);
	}
	MenuScreenSwitcher->SetActiveWidget(SettingsPanel);
	SettingsPanel->RefreshSettings();
	SetKeyboardFocus();
}

void UASInGameMenuWidget::ShowMainMenu()
{
	if (SettingsPanel) SettingsPanel->CloseKeyBindings();
	if (MenuScreenSwitcher) MenuScreenSwitcher->SetActiveWidgetIndex(0);
	SetKeyboardFocus();
}

void UASInGameMenuWidget::LeaveRoom()
{
	if (bLeavingRoom || (LeaveConfirmation && LeaveConfirmation->IsInViewport())) return;
	LeaveConfirmation = CreateWidget<UASLeaveRoomConfirmationWidget>(GetOwningPlayer());
	if (!LeaveConfirmation) return;
	LeaveConfirmation->SetReturnFocusWidget(this);
	LeaveConfirmation->OnConfirmed.BindUObject(this, &ThisClass::ConfirmLeaveRoom);
	LeaveConfirmation->AddToViewport(200);
	SetIsEnabled(false);
	LeaveConfirmation->SetKeyboardFocus();
}

void UASInGameMenuWidget::ConfirmLeaveRoom()
{
	if (bLeavingRoom || !GetGameInstance()) return;
	UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>();
	if (!Sessions) return;
	bLeavingRoom = true;
	if (ResumeButton) ResumeButton->SetIsEnabled(false);
	if (SettingsButton) SettingsButton->SetIsEnabled(false);
	if (LeaveRoomButton) LeaveRoomButton->SetIsEnabled(false);
	Sessions->LeaveRoom();
}
