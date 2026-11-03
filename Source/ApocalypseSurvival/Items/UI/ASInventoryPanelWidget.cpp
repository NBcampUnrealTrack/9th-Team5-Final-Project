#include "Items/UI/ASInventoryPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/Framework/ASPlayerController.h"
#include "InputCoreTypes.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ASInventorySlotWidget.h"

TSharedRef<SWidget> UASInventoryPanelWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UBorder* Shade = WidgetTree->ConstructWidget<UBorder>();
		Shade->SetBrushColor(FLinearColor(0, 0, 0, 0.55f));
		UCanvasPanelSlot* ShadeSlot = Canvas->AddChildToCanvas(Shade);
		ShadeSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		ShadeSlot->SetOffsets(FMargin(0));
		ShadeSlot->SetZOrder(-2);
		UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
		Scale->SetStretch(EStretch::ScaleToFit);
		Scale->SetStretchDirection(EStretchDirection::DownOnly);
		UCanvasPanelSlot* ScaleSlot = Canvas->AddChildToCanvas(Scale);
		ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		ScaleSlot->SetOffsets(FMargin(24));
		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
		Width->SetWidthOverride(828);
		Scale->SetContent(Width);
		WindowFrame = WidgetTree->ConstructWidget<UBorder>();
		WindowFrame->SetPadding(FMargin(20));
		WindowFrame->SetBrushColor(FLinearColor(0.025f, 0.03f, 0.04f, 0.99f));
		Width->SetContent(WindowFrame);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		WindowFrame->SetContent(Content);
		auto AddText = [&](const FText& Text, int32 FontSize)
		{
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
			Label->SetText(Text);
			FSlateFontInfo Font = Label->GetFont(); Font.Size = FontSize; Label->SetFont(Font);
			Content->AddChildToVerticalBox(Label)->SetPadding(FMargin(0, 5));
			return Label;
		};
		AddText(NSLOCTEXT("ASInventory", "Title", "인벤토리"), 24);
		CapacityText = AddText(FText::GetEmpty(), 14);
		UHorizontalBox* EquipmentRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		Content->AddChildToVerticalBox(EquipmentRow)->SetPadding(FMargin(0, 6));
		EquipmentPanel = WidgetTree->ConstructWidget<UUniformGridPanel>();
		EquipmentPanel->SetSlotPadding(FMargin(3));
		EquipmentRow->AddChildToHorizontalBox(EquipmentPanel);
		UVerticalBox* Actions = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* ActionsSlot = EquipmentRow->AddChildToHorizontalBox(Actions);
		ActionsSlot->SetPadding(FMargin(24, 4));
		ActionsSlot->SetVerticalAlignment(VAlign_Center);
		auto AddButton = [&](const FText& Text)
		{
			UButton* Button = WidgetTree->ConstructWidget<UButton>();
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(Text);
			FSlateFontInfo Font = Label->GetFont(); Font.Size = 16; Label->SetFont(Font);
			Button->SetContent(Label);
			Actions->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 4));
			return Button;
		};
		AddButton(NSLOCTEXT("ASInventory", "Crafting", "무기 제작"))->OnClicked.AddDynamic(this, &ThisClass::OpenCrafting);
		AddButton(NSLOCTEXT("ASInventory", "Close", "닫기  [I / Esc]"))->OnClicked.AddDynamic(this, &ThisClass::Close);
		AddText(NSLOCTEXT("ASInventory", "Storage", "가방 보관함"), 16);
		USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>();
		ScrollSize->SetMaxDesiredHeight(330);
		Content->AddChildToVerticalBox(ScrollSize);
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
		ScrollSize->SetContent(Scroll);
		SlotsPanel = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("SlotsPanel"));
		SlotsPanel->SetSlotPadding(FMargin(3));
		Scroll->AddChild(SlotsPanel);
		AddText(NSLOCTEXT("ASInventory", "QuickSlots", "퀵슬롯  [1–9 / 0]"), 16);
		QuickSlotsPanel = WidgetTree->ConstructWidget<UUniformGridPanel>();
		QuickSlotsPanel->SetSlotPadding(FMargin(3));
		Content->AddChildToVerticalBox(QuickSlotsPanel);
		AddText(NSLOCTEXT("ASInventory", "Controls", "클릭: 사용·장착   /   드래그: 이동   /   Ctrl+우클릭: 1개 버리기"), 12);
		FeedbackText = AddText(FText::GetEmpty(), 13);
		FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.3f)));
		FeedbackText->SetWrapTextAt(780);
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}

void UASInventoryPanelWidget::BuildSlotWidgets()
{
	if (!SlotsPanel || !QuickSlotsPanel || !EquipmentPanel) return;
	SlotsPanel->ClearChildren(); QuickSlotsPanel->ClearChildren(); EquipmentPanel->ClearChildren(); Slots.Reset();
	RenderedStorageCount = GetInventory() ? GetInventory()->GetStorageSlotCount() : UASInventoryComponent::BaseStorageSlotCount;
	auto AddSlot = [&](UUniformGridPanel* Grid, int32 Index, int32 Row, int32 Column)
	{
		UASInventorySlotWidget* Slot = CreateWidget<UASInventorySlotWidget>(GetOwningPlayer(),
			SlotWidgetClass ? SlotWidgetClass.Get() : UASInventorySlotWidget::StaticClass());
		if (!Slot) return;
		Slot->InitializeSlot(this, Index);
		Slots.Add(Slot);
		Grid->AddChildToUniformGrid(Slot, Row, Column);
	};
	AddSlot(EquipmentPanel, UASInventoryComponent::WeaponSlotIndex, 0, 0);
	AddSlot(EquipmentPanel, UASInventoryComponent::BagSlotIndex, 0, 1);
	for (int32 Index = 0; Index < RenderedStorageCount; ++Index)
		AddSlot(SlotsPanel, UASInventoryComponent::QuickSlotCount + Index, Index / 8, Index % 8);
	for (int32 Index = 0; Index < UASInventoryComponent::QuickSlotCount; ++Index)
		AddSlot(QuickSlotsPanel, Index, 0, Index);
}

void UASInventoryPanelWidget::RefreshSlots()
{
	if (FeedbackText) FeedbackText->SetText(FText::GetEmpty());
	const int32 Count = GetInventory() ? GetInventory()->GetStorageSlotCount() : UASInventoryComponent::BaseStorageSlotCount;
	if (Count != RenderedStorageCount) BuildSlotWidgets();
	Super::RefreshSlots();
	if (CapacityText) CapacityText->SetText(FText::Format(NSLOCTEXT("ASInventory", "Capacity", "보관함 {0}칸 + 퀵슬롯 10칸  ·  사용 중 {1}칸"),
		FText::AsNumber(Count), FText::AsNumber(GetInventory() ? GetInventory()->GetUsedSlotCount() : 0)));
}

UWidget* UASInventoryPanelWidget::GetInventoryBounds() const { return WindowFrame; }

void UASInventoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (FeedbackText) FeedbackText->SetText(FText::GetEmpty());
	if (AASPlayerController* PC = Cast<AASPlayerController>(GetOwningPlayer()))
		PC->GetItemInteractionComponent()->OnInventoryActionFailed.AddUniqueDynamic(this, &ThisClass::ShowActionFailure);
}

void UASInventoryPanelWidget::NativeDestruct()
{
	if (AASPlayerController* PC = Cast<AASPlayerController>(GetOwningPlayer()))
		PC->GetItemInteractionComponent()->OnInventoryActionFailed.RemoveDynamic(this, &ThisClass::ShowActionFailure);
	Super::NativeDestruct();
}

FReply UASInventoryPanelWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::I || Event.GetKey() == EKeys::Escape) { Close(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry, Event);
}

FReply UASInventoryPanelWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return FReply::Handled();
}

void UASInventoryPanelWidget::Close()
{
	if (AASPlayerController* PC = Cast<AASPlayerController>(GetOwningPlayer())) PC->GetItemInteractionComponent()->CloseInventory();
}

void UASInventoryPanelWidget::OpenCrafting()
{
	if (AASPlayerController* PC = Cast<AASPlayerController>(GetOwningPlayer())) { Close(); PC->ToggleCrafting(); }
}

void UASInventoryPanelWidget::ShowActionFailure(FText Message)
{
	if (FeedbackText) FeedbackText->SetText(Message);
}
