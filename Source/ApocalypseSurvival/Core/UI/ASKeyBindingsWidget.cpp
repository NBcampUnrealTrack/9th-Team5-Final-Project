#include "Core/UI/ASKeyBindingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/InputKeySelector.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/Settings/ASLocalSettingsSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputAction.h"

void UASKeyBindingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Root;
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChildToVerticalBox(Line);
	ActionLabel = WidgetTree->ConstructWidget<UTextBlock>();
	Line->AddChildToHorizontalBox(ActionLabel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	KeySelector = WidgetTree->ConstructWidget<UInputKeySelector>();
	KeySelector->SetAllowModifierKeys(false);
	KeySelector->SetAllowGamepadKeys(false);
	Line->AddChildToHorizontalBox(KeySelector)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	KeySelector->OnKeySelected.AddUniqueDynamic(this, &ThisClass::HandleKeySelected);
	ErrorLabel = WidgetTree->ConstructWidget<UTextBlock>();
	Root->AddChildToVerticalBox(ErrorLabel);
}

void UASKeyBindingRowWidget::SetMapping(UInputMappingContext* Context, const int32 Index)
{
	MappingContext = Context;
	MappingIndex = Index;
	if (!Context || !Context->GetMappings().IsValidIndex(Index)) return;
	const FEnhancedActionKeyMapping& Mapping = Context->GetMappings()[Index];
	FString Label = GetNameSafe(Mapping.Action);
	Label.RemoveFromStart(TEXT("IA_"));
	ActionLabel->SetText(FText::FromString(Label));
	if (const ULocalPlayer* Player = GetOwningLocalPlayer())
	{
		if (const UASLocalSettingsSubsystem* Settings = Player->GetSubsystem<UASLocalSettingsSubsystem>())
		{
			bUpdatingKey = true;
			KeySelector->SetSelectedKey(FInputChord(Settings->GetMappedKey(Context, Index)));
			bUpdatingKey = false;
		}
	}
}

void UASKeyBindingRowWidget::HandleKeySelected(const FInputChord Key)
{
	if (bUpdatingKey) return;
	ULocalPlayer* Player = GetOwningLocalPlayer();
	UASLocalSettingsSubsystem* Settings = Player ? Player->GetSubsystem<UASLocalSettingsSubsystem>() : nullptr;
	if (!Settings) return;
	FText Error;
	if (Key.bCtrl || Key.bAlt || Key.bShift || Key.bCmd)
	{
		Error = FText::FromString(TEXT("보조키 조합 없이 키 하나를 선택해 주세요."));
	}
	else
	{
		Settings->SetMappedKey(MappingContext, MappingIndex, Key.Key, Error);
	}
	ErrorLabel->SetText(Error);
	SetMapping(MappingContext, MappingIndex);
}

void UASKeyBindingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.98f));
	Background->SetPadding(FMargin(40.0f));
	WidgetTree->RootWidget = Background;
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Background->SetContent(Layout);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(FText::FromString(TEXT("키 바인딩 - 변경 즉시 저장됩니다.")));
	Layout->AddChildToVerticalBox(Title);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Layout->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Rows);
	UButton* Reset = WidgetTree->ConstructWidget<UButton>();
	UTextBlock* ResetText = WidgetTree->ConstructWidget<UTextBlock>();
	ResetText->SetText(FText::FromString(TEXT("기본 키로 복원")));
	Reset->SetContent(ResetText);
	Reset->OnClicked.AddUniqueDynamic(this, &ThisClass::ResetKeys);
	Layout->AddChildToVerticalBox(Reset);
	UButton* Close = WidgetTree->ConstructWidget<UButton>();
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>();
	CloseText->SetText(FText::FromString(TEXT("뒤로 가기")));
	Close->SetContent(CloseText);
	Close->OnClicked.AddUniqueDynamic(this, &ThisClass::ClosePanel);
	Layout->AddChildToVerticalBox(Close);
}

void UASKeyBindingsWidget::SetMappingContext(UInputMappingContext* Context)
{
	MappingContext = Context;
	RebuildRows();
}

void UASKeyBindingsWidget::RebuildRows()
{
	if (!Rows || !MappingContext) return;
	Rows->ClearChildren();
	for (int32 Index = 0; Index < MappingContext->GetMappings().Num(); ++Index)
	{
		const FEnhancedActionKeyMapping& Mapping = MappingContext->GetMappings()[Index];
		if (!Mapping.Action || !Mapping.Key.IsValid() || Mapping.Key.IsGamepadKey() || Mapping.Key.IsAnalog()) continue;
		UASKeyBindingRowWidget* Row = CreateWidget<UASKeyBindingRowWidget>(GetOwningPlayer());
		if (!Row) continue;
		Row->SetMapping(MappingContext, Index);
		Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 6.0f));
	}
}

void UASKeyBindingsWidget::ResetKeys()
{
	if (ULocalPlayer* Player = GetOwningLocalPlayer())
	{
		if (UASLocalSettingsSubsystem* Settings = Player->GetSubsystem<UASLocalSettingsSubsystem>()) Settings->ResetMappedKeys(MappingContext);
	}
	RebuildRows();
}

void UASKeyBindingsWidget::ClosePanel()
{
	RemoveFromParent();
}
