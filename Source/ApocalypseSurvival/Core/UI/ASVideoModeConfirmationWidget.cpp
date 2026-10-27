#include "Core/UI/ASVideoModeConfirmationWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"

UASVideoModeConfirmationWidget::UASVideoModeConfirmationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UASVideoModeConfirmationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(480.0f);
	Background->SetContent(Size);
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Size->SetContent(Layout);
	CountdownText = WidgetTree->ConstructWidget<UTextBlock>();
	CountdownText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(CountdownText);
	auto AddButton = [this, Layout](const TCHAR* Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Label));
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Button->SetContent(Text);
		Layout->AddChildToVerticalBox(Button);
		return Button;
	};
	AddButton(TEXT("변경 유지"))->OnClicked.AddUniqueDynamic(this, &ThisClass::Confirm);
	AddButton(TEXT("이전 화면 설정으로 복구"))->OnClicked.AddUniqueDynamic(this, &ThisClass::Revert);
}

void UASVideoModeConfirmationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Deadline = FPlatformTime::Seconds() + 15.0;
	if (UUserWidget* OwnerWidget = SettingsOwner.Get()) OwnerWidget->SetIsEnabled(false);
	SetKeyboardFocus();
}

void UASVideoModeConfirmationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(Deadline - FPlatformTime::Seconds()));
	CountdownText->SetText(FText::FromString(FString::Printf(TEXT("변경한 화면 설정을 유지하시겠습니까? %d초 후 자동 복구됩니다."), Seconds)));
	if (Seconds == 0) Revert();
}

void UASVideoModeConfirmationWidget::Confirm() { Resolve(true); }
void UASVideoModeConfirmationWidget::Revert() { Resolve(false); }

void UASVideoModeConfirmationWidget::Resolve(const bool bKeep)
{
	if (bResolved) return;
	bResolved = true;
	if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		if (bKeep) Settings->ConfirmVideoMode();
		else
		{
			Settings->RevertVideoMode();
			Settings->ApplyResolutionSettings(false);
		}
		Settings->SaveSettings();
	}
	RemoveFromParent();
}

void UASVideoModeConfirmationWidget::NativeDestruct()
{
	// Closing the owning menu must also restore an unconfirmed display mode.
	if (!bResolved)
	{
		bResolved = true;
		if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
		{
			Settings->RevertVideoMode();
			Settings->ApplyResolutionSettings(false);
			Settings->SaveSettings();
		}
	}
	if (UUserWidget* OwnerWidget = SettingsOwner.Get()) OwnerWidget->SetIsEnabled(true);
	OnClosed.ExecuteIfBound();
	OnClosed.Unbind();
	Super::NativeDestruct();
}

FReply UASVideoModeConfirmationWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Revert();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
