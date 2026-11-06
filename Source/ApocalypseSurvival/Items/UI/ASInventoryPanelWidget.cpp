#include "Items/UI/ASInventoryPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Quests/UI/ASQuestInventoryWidget.h"
#include "Modules/UI/ASModuleLoadoutWidget.h"
#include "Items/UI/ASMenuStyle.h"
#include "Items/UI/ViewModels/ASPlayerMenuViewModel.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "InputCoreTypes.h"
#include "Core/Framework/ASPlayerController.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ViewModels/ASInventoryViewModel.h"
#include "Items/UI/ASInventorySlotWidget.h"
#include "Items/UI/ASItemViewBinding.h"

UASInventoryPanelWidget::UASInventoryPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	QuickSlotsPerRow = 10;
}

void UASInventoryPanelWidget::BuildSlotWidgets()
{
	if (!SlotsPanel || !QuickSlotsPanel || !EquipmentPanel) return;
	SlotsPanel->ClearChildren(); QuickSlotsPanel->ClearChildren(); EquipmentPanel->ClearChildren(); Slots.Reset();
	RenderedStorageCount = GetViewModel() ? GetViewModel()->Presentation.StorageSlotCount : UASInventoryComponent::BaseStorageSlotCount;
	auto AddSlot = [&](UUniformGridPanel* Grid, int32 Index, int32 Row, int32 Column)
	{
		UASInventorySlotWidget* Slot = CreateSlotWidget(Index);
		if (!Slot) return;
		Grid->AddChildToUniformGrid(Slot, Row, Column);
	};
	AddSlot(EquipmentPanel, UASInventoryComponent::WeaponSlotIndex, 0, 0);
	AddSlot(EquipmentPanel, UASInventoryComponent::BagSlotIndex, 1, 0);
	for (int32 Index = 0; Index < RenderedStorageCount; ++Index)
		AddSlot(SlotsPanel, UASInventoryComponent::QuickSlotCount + Index, Index / FMath::Max(1, StorageColumns), Index % FMath::Max(1, StorageColumns));
	for (int32 Index = 0; Index < UASInventoryComponent::QuickSlotCount; ++Index)
		AddSlot(QuickSlotsPanel, Index, Index / FMath::Max(1, QuickSlotsPerRow), Index % FMath::Max(1, QuickSlotsPerRow));
}

void UASInventoryPanelWidget::RefreshSlots()
{
	const FASInventoryPresentation P = GetViewModel() ? GetViewModel()->Presentation : FASInventoryPresentation();
	if (P.StorageSlotCount != RenderedStorageCount) BuildSlotWidgets();
	Super::RefreshSlots();
	if (!ASItemViewBinding::HasView(this))
	{
		if (CapacityText) CapacityText->SetText(P.Capacity);
		if (FeedbackText) FeedbackText->SetText(P.Feedback);
	}
}

UWidget* UASInventoryPanelWidget::GetInventoryBounds() const { return WindowFrame; }

void UASInventoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (CloseButton) CloseButton->SetVisibility(ESlateVisibility::Collapsed);
	if (CraftingButton) CraftingButton->SetVisibility(ESlateVisibility::Collapsed);
	if (GetViewModel()) GetViewModel()->ClearFeedback();
	if (MenuTabs && MenuTabs->GetViewModel())
	{
		UASPlayerMenuViewModel* Menu = MenuTabs->GetViewModel();
		Menu->RemoveAllFieldValueChangedDelegates(this);
		Menu->AddFieldValueChangedDelegate(UASPlayerMenuViewModel::FFieldNotificationClassDescriptor::Presentation,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this, [this](UObject*, UE::FieldNotification::FFieldId) { RefreshMenuContent(); }));
	}
	ShowInventoryTab();
	RefreshMenuContent();
}

void UASInventoryPanelWidget::NativeDestruct()
{
	if (MenuTabs && MenuTabs->GetViewModel()) MenuTabs->GetViewModel()->RemoveAllFieldValueChangedDelegates(this);
	if (CloseButton) CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::Close);
	if (CraftingButton) CraftingButton->OnClicked.RemoveDynamic(this, &ThisClass::OpenCrafting);
	Super::NativeDestruct();
}

FReply UASInventoryPanelWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const AASPlayerController* PC = Cast<AASPlayerController>(GetOwningPlayer());
	if (Event.GetKey() == EKeys::Escape || (PC && PC->IsInventoryToggleKey(Event.GetKey())))
	{
		if (!Event.IsRepeat()) Close();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UASInventoryPanelWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return FReply::Handled();
}

void UASInventoryPanelWidget::Close()
{
	if (GetViewModel()) GetViewModel()->CloseInventory();
}

void UASInventoryPanelWidget::OpenCrafting()
{
	if (GetViewModel()) GetViewModel()->OpenCrafting();
}

TSharedRef<SWidget> UASInventoryPanelWidget::RebuildWidget()
{
	if (!IsDesignTime() && WindowFrame && !TabContent && WindowFrame->GetContent())
	{
		ASMenuStyle::StyleAuthoredTree(WidgetTree);
		ASMenuStyle::StyleWindow(WindowFrame);
		WindowFrame->SetContent(nullptr);
		UVerticalBox* Shell = WidgetTree->ConstructWidget<UVerticalBox>();
		MenuTabs = CreateWidget<UASPlayerMenuTabsWidget>(this);
		MenuTabs->ConfigureScreen(EASPlayerMenuTab::Inventory);
		Shell->AddChildToVerticalBox(MenuTabs);
		TabContent = WidgetTree->ConstructWidget<UWidgetSwitcher>();
		UBorder* InventoryPage = WidgetTree->ConstructWidget<UBorder>();
		InventoryPage->SetBrushColor(FLinearColor::Transparent); InventoryPage->SetPadding(FMargin(28.f, 24.f));
		UVerticalBox* InventoryBody = WidgetTree->ConstructWidget<UVerticalBox>(); InventoryPage->SetContent(InventoryBody);
		auto Heading = [&](const TCHAR* Label, int32 Size, FLinearColor Color)
		{
			UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(); Block->SetText(FText::FromString(Label));
			ASMenuStyle::StyleText(Block, Size, Color, true); return Block;
		};
		InventoryBody->AddChildToVerticalBox(Heading(TEXT("인벤토리"), 24, ASMenuStyle::Text))->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		UHorizontalBox* MainRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		InventoryBody->AddChildToVerticalBox(MainRow)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		USizeBox* EquipmentWidth = WidgetTree->ConstructWidget<USizeBox>(); EquipmentWidth->SetWidthOverride(168.f);
		UVerticalBox* EquipmentBody = WidgetTree->ConstructWidget<UVerticalBox>(); EquipmentWidth->SetContent(EquipmentBody);
		MainRow->AddChildToHorizontalBox(EquipmentWidth)->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		EquipmentBody->AddChildToVerticalBox(Heading(TEXT("장비"), 12, ASMenuStyle::Accent))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		EquipmentPanel->RemoveFromParent(); EquipmentPanel->SetSlotPadding(FMargin(3.f));
		EquipmentBody->AddChildToVerticalBox(EquipmentPanel)->SetHorizontalAlignment(HAlign_Left);
		UVerticalBox* StorageBody = WidgetTree->ConstructWidget<UVerticalBox>();
		MainRow->AddChildToHorizontalBox(StorageBody)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StorageBody->AddChildToVerticalBox(Heading(TEXT("보관함"), 12, ASMenuStyle::Accent))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		CapacityText->RemoveFromParent(); ASMenuStyle::StyleText(CapacityText, 12, ASMenuStyle::Muted);
		StorageBody->AddChildToVerticalBox(CapacityText)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		UScrollBox* StorageScroll = WidgetTree->ConstructWidget<UScrollBox>();
		StorageBody->AddChildToVerticalBox(StorageScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotsPanel->RemoveFromParent(); SlotsPanel->SetSlotPadding(FMargin(3.f));
		UHorizontalBox* GridAlign = WidgetTree->ConstructWidget<UHorizontalBox>(); GridAlign->AddChildToHorizontalBox(SlotsPanel); StorageScroll->AddChild(GridAlign);
		InventoryBody->AddChildToVerticalBox(Heading(TEXT("퀵슬롯"), 12, ASMenuStyle::Accent))->SetPadding(FMargin(0.f, 22.f, 0.f, 12.f));
		QuickSlotsPanel->RemoveFromParent(); QuickSlotsPanel->SetSlotPadding(FMargin(3.f));
		InventoryBody->AddChildToVerticalBox(QuickSlotsPanel)->SetHorizontalAlignment(HAlign_Left);
		UTextBlock* Hint = Heading(TEXT("클릭: 사용·장착     드래그: 이동     Ctrl + 우클릭: 1개 버리기"), 11, ASMenuStyle::Muted);
		InventoryBody->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		FeedbackText->RemoveFromParent(); ASMenuStyle::StyleText(FeedbackText, 12, ASMenuStyle::Danger);
		InventoryBody->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		TabContent->AddChild(InventoryPage);
		for (UUserWidget* Page : {static_cast<UUserWidget*>(CreateWidget<UASQuestInventoryWidget>(this)), static_cast<UUserWidget*>(CreateWidget<UASModuleLoadoutWidget>(this))})
		{
			UBorder* PageFrame = WidgetTree->ConstructWidget<UBorder>();
			PageFrame->SetBrushColor(FLinearColor::Transparent); PageFrame->SetPadding(FMargin(28.f, 24.f));
			PageFrame->SetContent(Page); TabContent->AddChild(PageFrame);
		}
		Shell->AddChildToVerticalBox(TabContent)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		WindowFrame->SetContent(Shell);
	}
	return Super::RebuildWidget();
}

void UASInventoryPanelWidget::ShowInventoryTab()
{
	SelectMenuTab(EASPlayerMenuTab::Inventory);
}

void UASInventoryPanelWidget::ShowQuestTab()
{
	SelectMenuTab(EASPlayerMenuTab::Quest);
}

void UASInventoryPanelWidget::SelectMenuTab(EASPlayerMenuTab Tab)
{
	if (MenuTabs && MenuTabs->GetViewModel()) MenuTabs->GetViewModel()->RequestTab(Tab);
}

void UASInventoryPanelWidget::RefreshMenuContent()
{
	if (!TabContent || !MenuTabs || !MenuTabs->GetViewModel()) return;
	const int32 Index = MenuTabs->GetViewModel()->Presentation.ContentIndex;
	if (Index >= 0 && Index < TabContent->GetNumWidgets()) TabContent->SetActiveWidgetIndex(Index);
}
