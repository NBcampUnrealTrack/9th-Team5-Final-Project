#include "Items/UI/ASPlayerMenuTabsWidget.h"
#include "Items/UI/ASMenuStyle.h"
#include "Items/UI/ASItemViewBinding.h"
#include "Items/UI/ViewModels/ASPlayerMenuViewModel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void UASPlayerMenuTabsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ViewModel = NewObject<UASPlayerMenuViewModel>(this);
	ViewModel->Initialize(GetOwningPlayer(), EASPlayerMenuTab::Inventory);
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Root;
	UBorder* Header = WidgetTree->ConstructWidget<UBorder>();
	Header->SetBrushColor(ASMenuStyle::Background);
	Header->SetPadding(FMargin(20.f, 8.f));
	Root->AddChildToVerticalBox(Header);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Header->SetContent(Row);
	for (int32 Index = 0; Index < ViewModel->Presentation.Tabs.Num(); ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("MenuTab%d"), Index));
		ASMenuStyle::StyleButton(Button);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		Button->SetContent(Content);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(ViewModel->Presentation.Tabs[Index].Label);
		ASMenuStyle::StyleText(Label, 16);
		Content->AddChildToVerticalBox(Label);
		UBorder* Mark = WidgetTree->ConstructWidget<UBorder>();
		Mark->SetPadding(FMargin(0.f, 1.f));
		Content->AddChildToVerticalBox(Mark);
		Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
		Buttons.Add(Button); Labels.Add(Label); Indicators.Add(Mark);
	}
	Buttons[0]->OnClicked.AddDynamic(this, &ThisClass::Inventory);
	Buttons[1]->OnClicked.AddDynamic(this, &ThisClass::Quest);
	Buttons[2]->OnClicked.AddDynamic(this, &ThisClass::Modules);
	Buttons[3]->OnClicked.AddDynamic(this, &ThisClass::Crafting);
	Row->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UButton* Exit = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MenuClose"));
	ASMenuStyle::StyleButton(Exit);
	UTextBlock* ExitText = WidgetTree->ConstructWidget<UTextBlock>();
	ExitText->SetText(NSLOCTEXT("PlayerMenu", "Close", "닫기"));
	ASMenuStyle::StyleText(ExitText, 13, ASMenuStyle::Muted);
	Exit->SetContent(ExitText);
	Exit->OnClicked.AddDynamic(this, &ThisClass::Close);
	Row->AddChildToHorizontalBox(Exit);
	UBorder* Rule = WidgetTree->ConstructWidget<UBorder>();
	Rule->SetBrushColor(ASMenuStyle::Line); Rule->SetPadding(FMargin(0.f, .5f));
	Root->AddChildToVerticalBox(Rule);
	Refresh();
}

void UASPlayerMenuTabsWidget::NativeConstruct()
{
	ViewModel->Initialize(GetOwningPlayer(), ViewModel->GetScreen());
	Super::NativeConstruct();
	ASItemViewBinding::Set(this, TEXT("MenuVM"), ViewModel);
	ViewModel->RemoveAllFieldValueChangedDelegates(this);
	ViewModel->AddFieldValueChangedDelegate(UASPlayerMenuViewModel::FFieldNotificationClassDescriptor::Presentation,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this, [this](UObject*, UE::FieldNotification::FFieldId) { Refresh(); }));
	Refresh();
}

void UASPlayerMenuTabsWidget::NativeDestruct()
{
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	Super::NativeDestruct();
}

void UASPlayerMenuTabsWidget::ConfigureScreen(EASPlayerMenuTab Screen)
{
	if (ViewModel) ViewModel->Initialize(GetOwningPlayer(), Screen);
	Refresh();
}

void UASPlayerMenuTabsWidget::SetActiveTab(EASPlayerMenuTab Tab)
{
	if (ViewModel) ViewModel->SetActiveTab(Tab);
	Refresh();
}

void UASPlayerMenuTabsWidget::Refresh()
{
	if (!ViewModel) return;
	for (int32 Index = 0; Index < Buttons.Num() && ViewModel->Presentation.Tabs.IsValidIndex(Index); ++Index)
	{
		const FASPlayerMenuTabPresentation& Entry = ViewModel->Presentation.Tabs[Index];
		const bool bActive = Entry.bActive;
		Labels[Index]->SetText(Entry.Label);
		ASMenuStyle::StyleText(Labels[Index], 16, bActive ? ASMenuStyle::Text : ASMenuStyle::Muted, bActive);
		Indicators[Index]->SetBrushColor(bActive ? ASMenuStyle::Accent : FLinearColor::Transparent);
		Buttons[Index]->SetBackgroundColor(bActive ? FLinearColor::White : FLinearColor(.65f, .65f, .65f, 1.f));
	}
}

void UASPlayerMenuTabsWidget::Inventory() { ViewModel->RequestTab(EASPlayerMenuTab::Inventory); OnTabSelected.Broadcast(EASPlayerMenuTab::Inventory); }
void UASPlayerMenuTabsWidget::Quest() { ViewModel->RequestTab(EASPlayerMenuTab::Quest); OnTabSelected.Broadcast(EASPlayerMenuTab::Quest); }
void UASPlayerMenuTabsWidget::Modules() { ViewModel->RequestTab(EASPlayerMenuTab::Modules); OnTabSelected.Broadcast(EASPlayerMenuTab::Modules); }
void UASPlayerMenuTabsWidget::Crafting() { ViewModel->RequestTab(EASPlayerMenuTab::Crafting); OnTabSelected.Broadcast(EASPlayerMenuTab::Crafting); }
void UASPlayerMenuTabsWidget::Close() { ViewModel->RequestClose(); OnCloseRequested.Broadcast(); }
