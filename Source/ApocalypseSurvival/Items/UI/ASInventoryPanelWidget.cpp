#include "Items/UI/ASInventoryPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Quests/UI/ASQuestInventoryWidget.h"

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
	AddSlot(EquipmentPanel, UASInventoryComponent::BagSlotIndex, 0, 1);
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
	if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Close);
	if (CraftingButton) CraftingButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenCrafting);
	if (GetViewModel()) GetViewModel()->ClearFeedback();
	ShowInventoryTab();
}

void UASInventoryPanelWidget::NativeDestruct()
{
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
		UWidget* InventoryBody = WindowFrame->GetContent();
		WindowFrame->SetContent(nullptr);
		UVerticalBox* Shell = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
		Shell->AddChildToVerticalBox(Header);
		const auto Button = [&](const TCHAR* Label)
		{
			UButton* Result = WidgetTree->ConstructWidget<UButton>();
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
			Text->SetText(FText::FromString(Label)); Text->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
			Result->SetContent(Text); Header->AddChildToHorizontalBox(Result); return Result;
		};
		InventoryTabButton = Button(TEXT("  인벤토리  "));
		InventoryTabButton->OnClicked.AddDynamic(this, &ThisClass::ShowInventoryTab);
		Button(TEXT("  제작  "))->OnClicked.AddDynamic(this, &ThisClass::OpenCrafting);
		QuestTabButton = Button(TEXT("  퀘스트  "));
		QuestTabButton->OnClicked.AddDynamic(this, &ThisClass::ShowQuestTab);
		UButton* Module = Button(TEXT("  모듈  "));
		Module->SetIsEnabled(false); Module->SetToolTipText(FText::FromString(TEXT("모듈 설정 화면 연결 예정")));
		Button(TEXT("  닫기  "))->OnClicked.AddDynamic(this, &ThisClass::Close);
		TabContent = WidgetTree->ConstructWidget<UWidgetSwitcher>();
		TabContent->AddChild(InventoryBody);
		TabContent->AddChild(CreateWidget<UASQuestInventoryWidget>(GetOwningPlayer()));
		Shell->AddChildToVerticalBox(TabContent);
		WindowFrame->SetContent(Shell);
	}
	return Super::RebuildWidget();
}

void UASInventoryPanelWidget::ShowInventoryTab()
{
	if (TabContent) TabContent->SetActiveWidgetIndex(0);
	if (InventoryTabButton) InventoryTabButton->SetBackgroundColor(FLinearColor(.4f,.8f,1.f));
	if (QuestTabButton) QuestTabButton->SetBackgroundColor(FLinearColor::White);
}

void UASInventoryPanelWidget::ShowQuestTab()
{
	if (TabContent) TabContent->SetActiveWidgetIndex(1);
	if (QuestTabButton) QuestTabButton->SetBackgroundColor(FLinearColor(.4f,.8f,1.f));
	if (InventoryTabButton) InventoryTabButton->SetBackgroundColor(FLinearColor::White);
}
