#include "Items/UI/ASInventoryPanelWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "InputCoreTypes.h"
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
}

void UASInventoryPanelWidget::NativeDestruct()
{
	if (CloseButton) CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::Close);
	if (CraftingButton) CraftingButton->OnClicked.RemoveDynamic(this, &ThisClass::OpenCrafting);
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
	if (GetViewModel()) GetViewModel()->CloseInventory();
}

void UASInventoryPanelWidget::OpenCrafting()
{
	if (GetViewModel()) GetViewModel()->OpenCrafting();
}
