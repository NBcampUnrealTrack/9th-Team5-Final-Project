#include "Items/UI/ASInventoryBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/UniformGridPanel.h"
#include "GameFramework/PlayerController.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ASInventorySlotWidget.h"

TSharedRef<SWidget> UASInventoryBarWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SlotsPanel = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("SlotsPanel"));
		SlotsPanel->SetSlotPadding(FMargin(3.0f));
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(SlotsPanel);
		PanelSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		PanelSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		PanelSlot->SetPosition(FVector2D(-24.0f, -24.0f));
		PanelSlot->SetAutoSize(true);
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}

void UASInventoryBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (SlotsPanel)
	{
		SlotsPanel->ClearChildren();
		Slots.Reset();
		for (int32 Index = 0; Index < UASInventoryComponent::PlayerSlotCount; ++Index)
		{
			UASInventorySlotWidget* Entry = CreateWidget<UASInventorySlotWidget>(GetOwningPlayer(),
				SlotWidgetClass ? SlotWidgetClass.Get() : UASInventorySlotWidget::StaticClass());
			if (!Entry) continue;
			Entry->InitializeSlot(this, Index);
			Slots.Add(Entry);
			SlotsPanel->AddChildToUniformGrid(Entry, Index / 5, Index % 5);
		}
	}
	if (Inventory) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshSlots);
	RefreshSlots();
}

void UASInventoryBarWidget::NativeDestruct()
{
	if (Inventory) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshSlots);
	Super::NativeDestruct();
}

void UASInventoryBarWidget::SetInventory(UASInventoryComponent* InInventory)
{
	if (Inventory) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshSlots);
	Inventory = InInventory;
	if (Inventory) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshSlots);
	RefreshSlots();
}

void UASInventoryBarWidget::RefreshSlots()
{
	for (UASInventorySlotWidget* Entry : Slots) if (IsValid(Entry)) Entry->RefreshSlot();
}

void UASInventoryBarWidget::ActivateSlot(const int32 SlotIndex, const FGuid ExpectedItem)
{
	if (GetOwningPlayer())
	{
		if (UASItemInteractionComponent* Interaction = GetOwningPlayer()->FindComponentByClass<UASItemInteractionComponent>())
			Interaction->RequestActivateSlot(SlotIndex, ExpectedItem);
	}
}

void UASInventoryBarWidget::SwapSlots(const int32 FromSlot, const int32 ToSlot,
	const FGuid ExpectedFrom, const FGuid ExpectedTo)
{
	if (GetOwningPlayer())
	{
		if (UASItemInteractionComponent* Interaction = GetOwningPlayer()->FindComponentByClass<UASItemInteractionComponent>())
			Interaction->RequestSwapSlots(FromSlot, ToSlot, ExpectedFrom, ExpectedTo);
	}
}
