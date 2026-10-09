#include "Items/UI/ASInventorySlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ASInventoryBarWidget.h"

void UASInventorySlotDragDrop::Drop_Implementation(const FPointerEvent& PointerEvent)
{
	if (SourceBar) SourceBar->EndItemDrag();
	Super::Drop_Implementation(PointerEvent);
}

void UASInventorySlotDragDrop::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	if (SourceBar) SourceBar->EndItemDrag();
	Super::DragCancelled_Implementation(PointerEvent);
}

TSharedRef<SWidget> UASInventorySlotWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(72.0f);
		Size->SetHeightOverride(76.0f);
		SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotBorder"));
		SlotBorder->SetPadding(FMargin(4.0f));
		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
		Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
		ItemIcon->SetDesiredSizeOverride(FVector2D(44.0f));
		UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(ItemIcon);
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		auto AddText = [&](const TCHAR* Name, EHorizontalAlignment Horizontal, EVerticalAlignment Vertical)
		{
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
			FSlateFontInfo Font = Text->GetFont();
			Font.Size = 11;
			Text->SetFont(Font);
			Text->SetVisibility(ESlateVisibility::HitTestInvisible);
			UOverlaySlot* Slot = Overlay->AddChildToOverlay(Text);
			Slot->SetHorizontalAlignment(Horizontal);
			Slot->SetVerticalAlignment(Vertical);
			return Text;
		};
		KeyText = AddText(TEXT("KeyText"), HAlign_Left, VAlign_Top);
		ItemNameText = AddText(TEXT("ItemNameText"), HAlign_Center, VAlign_Center);
		QuantityText = AddText(TEXT("QuantityText"), HAlign_Right, VAlign_Bottom);
		EquippedText = AddText(TEXT("EquippedText"), HAlign_Left, VAlign_Bottom);
		SlotBorder->SetContent(Overlay);
		Size->SetContent(SlotBorder);
		WidgetTree->RootWidget = Size;
	}
	return Super::RebuildWidget();
}

void UASInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	RefreshSlot();
}

void UASInventorySlotWidget::InitializeSlot(UASInventoryBarWidget* InBar, const int32 InSlotIndex)
{
	Bar = InBar;
	SlotIndex = InSlotIndex;
	RefreshSlot();
}

UASItemInstance* UASInventorySlotWidget::GetItem() const
{
	UASInventoryComponent* Inventory = Bar ? Bar->GetInventory() : nullptr;
	return IsValid(Inventory) ? Inventory->GetItemAtSlot(SlotIndex) : nullptr;
}

void UASInventorySlotWidget::RefreshSlot()
{
	UASItemInstance* Item = GetItem();
	const UASItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	UTexture2D* Icon = Definition ? Definition->Icon.LoadSynchronous() : nullptr;
	const bool bEquipped = Item && Bar->GetInventory()->GetEquippedItem() == Item;
	if (KeyText) KeyText->SetText(FText::AsNumber((SlotIndex + 1) % 10));
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(Icon);
		ItemIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ItemNameText) ItemNameText->SetText(Definition && !Icon ? Definition->DisplayName : FText::GetEmpty());
	if (QuantityText) QuantityText->SetText(Item && Item->GetQuantity() > 1 ? FText::AsNumber(Item->GetQuantity()) : FText::GetEmpty());
	if (EquippedText) EquippedText->SetText(bEquipped ? FText::FromString(TEXT("E")) : FText::GetEmpty());
	if (SlotBorder) SlotBorder->SetBrushColor(bEquipped ? FLinearColor(0.15f, 0.4f, 0.65f, 0.95f) : FLinearColor(0.08f, 0.1f, 0.13f, 0.95f));
	SetToolTipText(Definition ? Definition->DisplayName : FText::GetEmpty());
}

FReply UASInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return Super::NativeOnMouseButtonDown(Geometry, Event);
	bPressed = true;
	PressedItem = GetItem() ? GetItem()->GetInstanceId() : FGuid();
	FReply Reply = FReply::Handled().CaptureMouse(TakeWidget());
	return PressedItem.IsValid() ? Reply.DetectDrag(TakeWidget(), EKeys::LeftMouseButton) : Reply;
}

FReply UASInventorySlotWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return Super::NativeOnMouseButtonUp(Geometry, Event);
	if (bPressed && Bar && PressedItem.IsValid() && Geometry.IsUnderLocation(Event.GetScreenSpacePosition()))
		Bar->ActivateSlot(SlotIndex, PressedItem);
	bPressed = false;
	return FReply::Handled().ReleaseMouseCapture();
}

void UASInventorySlotWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	bPressed = false;
	UASItemInstance* Item = GetItem();
	if (!Bar || !Item || Item->GetInstanceId() != PressedItem) return;
	UASInventorySlotDragDrop* Drag = NewObject<UASInventorySlotDragDrop>();
	Drag->Inventory = Bar->GetInventory();
	Drag->SourceBar = Bar;
	Drag->FromSlot = SlotIndex;
	Drag->ItemId = PressedItem;
	UImage* Visual = NewObject<UImage>(Drag);
	UTexture2D* Icon = Item->GetItemDefinition()->Icon.LoadSynchronous();
	Visual->SetBrushFromTexture(Icon);
	Visual->SetDesiredSizeOverride(FVector2D(44.0f));
	Visual->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Drag->DefaultDragVisual = Visual;
	Drag->Pivot = EDragPivot::CenterCenter;
	Bar->BeginItemDrag();
	Operation = Drag;
}

void UASInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	bPressed = false;
	Super::NativeOnDragCancelled(Event, Operation);
}

bool UASInventorySlotWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	UASInventorySlotDragDrop* Drag = Cast<UASInventorySlotDragDrop>(Operation);
	if (!Drag || !Bar || Drag->Inventory != Bar->GetInventory() || Drag->FromSlot == SlotIndex) return false;
	Bar->SwapSlots(Drag->FromSlot, SlotIndex, Drag->ItemId, GetItem() ? GetItem()->GetInstanceId() : FGuid());
	return true;
}
