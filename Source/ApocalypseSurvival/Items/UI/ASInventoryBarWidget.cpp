#include "Items/UI/ASInventoryBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/UniformGridPanel.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
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
	// Also supports BP layouts whose root is a full-screen CanvasPanel.
	if (WidgetTree && !OutsideDropArea)
	{
		if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		{
			OutsideDropArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OutsideDropArea"));
			OutsideDropArea->SetBrushColor(FLinearColor::Transparent);
			UCanvasPanelSlot* DropAreaSlot = Canvas->AddChildToCanvas(OutsideDropArea);
			DropAreaSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DropAreaSlot->SetOffsets(FMargin(0.0f));
			DropAreaSlot->SetZOrder(-1);
		}
	}
	EndItemDrag();
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
		for (int32 Index = -1; Index < UASInventoryComponent::PlayerSlotCount; ++Index)
		{
			UASInventorySlotWidget* Entry = CreateWidget<UASInventorySlotWidget>(GetOwningPlayer(),
				SlotWidgetClass ? SlotWidgetClass.Get() : UASInventorySlotWidget::StaticClass());
			if (!Entry) continue;
			Entry->InitializeSlot(this, Index < 0 ? UASInventoryComponent::WeaponSlotIndex : Index);
			Slots.Add(Entry);
			SlotsPanel->AddChildToUniformGrid(Entry, Index < 0 ? 0 : Index / 5, Index < 0 ? 0 : Index % 5 + 1);
		}
	}
	if (Inventory) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshSlots);
	RefreshSlots();
}

void UASInventoryBarWidget::NativeDestruct()
{
	EndItemDrag();
	if (Inventory) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshSlots);
	Super::NativeDestruct();
}

void UASInventoryBarWidget::BeginItemDrag()
{
	if (OutsideDropArea) OutsideDropArea->SetVisibility(ESlateVisibility::Visible);
}

void UASInventoryBarWidget::EndItemDrag()
{
	if (OutsideDropArea) OutsideDropArea->SetVisibility(ESlateVisibility::Collapsed);
}

bool UASInventoryBarWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	UASInventorySlotDragDrop* Drag = Cast<UASInventorySlotDragDrop>(Operation);
	if (!Drag || Drag->SourceBar != this || Drag->Inventory != Inventory || !IsValid(Inventory)) return false;
	// Dropping in gaps or back onto the source slot is a no-op, not a discard.
	if (!SlotsPanel || SlotsPanel->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition())) return true;
	if (GetOwningPlayer())
	{
		if (UASItemInteractionComponent* Interaction = GetOwningPlayer()->FindComponentByClass<UASItemInteractionComponent>())
			Interaction->RequestDropSlot(Drag->FromSlot, Drag->ItemId);
	}
	return true;
}

bool UASInventoryBarWidget::IsCursorOverInventory() const
{
	return IsInViewport() && IsVisible() && SlotsPanel && SlotsPanel->IsVisible()
		&& FSlateApplication::IsInitialized()
		&& SlotsPanel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
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

void UASInventoryBarWidget::DropOneFromSlot(const int32 SlotIndex, const FGuid ExpectedItem)
{
	if (GetOwningPlayer())
	{
		if (UASItemInteractionComponent* Interaction = GetOwningPlayer()->FindComponentByClass<UASItemInteractionComponent>())
			Interaction->RequestDropOneFromSlot(SlotIndex, ExpectedItem);
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
