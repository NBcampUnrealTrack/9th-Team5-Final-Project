#include "Items/UI/ASInventorySlotWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Items/UI/ViewModels/ASItemSlotViewModel.h"
#include "Items/UI/ViewModels/ASInventoryViewModel.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ASInventoryBarWidget.h"
#include "Items/UI/ASItemRarityBackground.h"
#include "Items/UI/ASItemViewBinding.h"
#include "Items/UI/ASMenuStyle.h"

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

void UASInventorySlotWidget::NativePreConstruct()
{
	if (IsDesignTime())
	{
		if (ItemNameText) ItemNameText->SetText(NSLOCTEXT("ASInventory", "PreviewItem", "아이템"));
		if (QuantityText) QuantityText->SetText(FText::AsNumber(12));
		if (KeyText) KeyText->SetText(SlotIndex == UASInventoryComponent::WeaponSlotIndex ? NSLOCTEXT("Items", "WeaponSlot", "무기")
			: SlotIndex == UASInventoryComponent::BagSlotIndex ? NSLOCTEXT("Items", "BagSlot", "가방")
			: SlotIndex >= 0 && SlotIndex < UASInventoryComponent::QuickSlotCount ? FText::AsNumber((SlotIndex + 1) % 10) : FText::GetEmpty());
	}
	Super::NativePreConstruct();
}

void UASInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (SlotBorder) SlotBorder->SetBrushColor(ASMenuStyle::Surface);
	DefaultSlotColor = ASMenuStyle::Surface; bCapturedSlotColor = true;
	ASMenuStyle::StyleText(KeyText, 10, ASMenuStyle::Muted);
	ASMenuStyle::StyleText(ItemNameText, 11, ASMenuStyle::Text);
	ASMenuStyle::StyleText(QuantityText, 12, ASMenuStyle::Text);
	ASMenuStyle::StyleText(EquippedText, 10, ASMenuStyle::Accent);
	SetIsFocusable(false);
	RefreshSlot();
}

void UASInventorySlotWidget::InitializeSlot(UASInventoryBarWidget* InBar, const int32 InSlotIndex)
{
	Bar = InBar;
	SlotIndex = InSlotIndex;
	RefreshSlot();
}

void UASInventorySlotWidget::NativeDestruct()
{
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	Super::NativeDestruct();
}

void UASInventorySlotWidget::RefreshSlot()
{
	UASItemSlotViewModel* Model = Bar && Bar->GetViewModel() ? Bar->GetViewModel()->GetSlot(SlotIndex) : nullptr;
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	ViewModel = Model;
	if (ViewModel) ViewModel->AddFieldValueChangedDelegate(UASItemSlotViewModel::FFieldNotificationClassDescriptor::Presentation,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this,
			[this](UObject*, UE::FieldNotification::FFieldId) { ApplyPresentation(); }));
	ASItemViewBinding::Set(this, TEXT("SlotVM"), ViewModel);
	ApplyPresentation();
}

FReply UASInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (Event.IsControlDown() && Bar && SlotIndex >= 0)
		{
			if (ViewModel && ViewModel->Presentation.bHasItem)
				Bar->DropOneFromSlot(SlotIndex, ViewModel->Presentation.ItemId);
		}
		// All right clicks over slots belong to the UI, including empty and weapon slots.
		return FReply::Handled();
	}
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return Super::NativeOnMouseButtonDown(Geometry, Event);
	bPressed = true;
	PressedItem = ViewModel ? ViewModel->Presentation.ItemId : FGuid();
	FReply Reply = FReply::Handled().CaptureMouse(TakeWidget());
	return ViewModel && ViewModel->Presentation.bCanDrag ? Reply.DetectDrag(TakeWidget(), EKeys::LeftMouseButton) : Reply;
}

FReply UASInventorySlotWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton) return FReply::Handled();
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return Super::NativeOnMouseButtonUp(Geometry, Event);
	if (bPressed && Bar && PressedItem.IsValid() && Geometry.IsUnderLocation(Event.GetScreenSpacePosition()))
		Bar->ActivateSlot(SlotIndex, PressedItem);
	bPressed = false;
	return FReply::Handled().ReleaseMouseCapture();
}

void UASInventorySlotWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	bPressed = false;
	if (!Bar || !ViewModel || !ViewModel->Presentation.bCanDrag || ViewModel->Presentation.ItemId != PressedItem) return;
	UASInventorySlotDragDrop* Drag = NewObject<UASInventorySlotDragDrop>();
	Drag->Inventory = Bar->GetInventory();
	Drag->ViewModel = Bar->GetViewModel();
	Drag->SourceBar = Bar;
	Drag->FromSlot = SlotIndex;
	Drag->ItemId = PressedItem;
	UASInventorySlotWidget* Visual = CreateWidget<UASInventorySlotWidget>(GetOwningPlayer(), GetClass());
	if (!Visual) return;
	Visual->InitializeSlot(Bar, SlotIndex);
	Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
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
	if (!Drag || !Bar || !ViewModel || Drag->ViewModel != Bar->GetViewModel() || Drag->FromSlot == SlotIndex) return false;
	Bar->SwapSlots(Drag->FromSlot, SlotIndex, Drag->ItemId, ViewModel->Presentation.ItemId);
	return true;
}

void UASInventorySlotWidget::ApplyPresentation()
{
	const FASItemSlotPresentation P = ViewModel ? ViewModel->Presentation : FASItemSlotPresentation();
	if (RarityBackground) { RarityBackground->SetRarity(P.Rarity); RarityBackground->SetVisibility(P.bHasItem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	const bool bAuthoredBindings = ASItemViewBinding::HasView(this);
	if (!bAuthoredBindings && KeyText) KeyText->SetText(P.Key);
	if (ItemIcon) { ItemIcon->SetBrushFromTexture(P.Icon); ItemIcon->SetVisibility(P.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (ItemNameText)
	{
		if (!bAuthoredBindings) ItemNameText->SetText(P.Name);
		ItemNameText->SetVisibility(P.Icon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (!bAuthoredBindings && QuantityText) QuantityText->SetText(P.Quantity);
	if (!bAuthoredBindings && EquippedText) EquippedText->SetText(P.Equipped);
	if (SlotBorder)
	{
		if (!bCapturedSlotColor) { DefaultSlotColor = SlotBorder->GetBrushColor(); bCapturedSlotColor = true; }
		SlotBorder->SetBrushColor(P.bEquipped ? FLinearColor(.15f, .135f, .085f) : DefaultSlotColor);
	}
	SetToolTipText(P.Tooltip);
}
