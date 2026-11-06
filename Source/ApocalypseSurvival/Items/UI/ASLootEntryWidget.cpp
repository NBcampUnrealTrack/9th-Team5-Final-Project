#include "Items/UI/ASLootEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Items/UI/ViewModels/ASItemSlotViewModel.h"
#include "Items/UI/ViewModels/ASInventoryViewModel.h"
#include "Items/UI/ASItemRarityBackground.h"
#include "Items/UI/ASLootWidget.h"
#include "Items/UI/ASItemViewBinding.h"
#include "Items/Loot/ASLootChest.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UASLootEntryWidget::RebuildWidget()
{
	if (EntrySize)
	{
		// Constrain the visible card without changing the grid's desired row height during scrolling.
		EntrySize->SetMinAspectRatio(1.f);
		EntrySize->SetMaxAspectRatio(1.f);
	}
	// Keep the authored icon binding and layer order, but fit its texture inside the cell.
	if (ItemIcon && WidgetTree)
	{
		if (UOverlay* Overlay = Cast<UOverlay>(ItemIcon->GetParent()))
		{
			const int32 IconIndex = Overlay->GetChildIndex(ItemIcon);
			const UOverlaySlot* OldSlot = CastChecked<UOverlaySlot>(ItemIcon->Slot);
			const FMargin IconPadding = OldSlot->GetPadding();
			ItemIcon->RemoveFromParent();
			UScaleBox* IconScale = WidgetTree->ConstructWidget<UScaleBox>();
			IconScale->SetStretch(EStretch::ScaleToFit);
			IconScale->SetStretchDirection(EStretchDirection::Both);
			IconScale->SetContent(ItemIcon);
			UOverlaySlot* IconSlot = CastChecked<UOverlaySlot>(Overlay->InsertChildAt(IconIndex, IconScale));
			IconSlot->SetPadding(IconPadding);
			IconSlot->SetHorizontalAlignment(HAlign_Fill);
			IconSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	return Super::RebuildWidget();
}

void UASLootEntryWidget::NativePreConstruct()
{
	if (IsDesignTime())
	{
		if (ItemNameText) { ItemNameText->SetText(NSLOCTEXT("ASInventory", "PreviewItem", "아이템")); ItemNameText->SetVisibility(ESlateVisibility::HitTestInvisible); }
		if (QuantityText) { QuantityText->SetText(FText::AsNumber(12)); QuantityText->SetVisibility(ESlateVisibility::HitTestInvisible); }
		if (QuantityBadge) QuantityBadge->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	Super::NativePreConstruct();
}

void UASLootEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(ItemButton))
	{
		ItemButton->OnClicked.AddUniqueDynamic(this, &UASLootEntryWidget::HandleItemButtonClicked);
	}

	RefreshEntry();
}

void UASLootEntryWidget::NativeDestruct()
{
	bPressed = false;
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	if (IsValid(ItemButton))
	{
		ItemButton->OnClicked.RemoveDynamic(this, &UASLootEntryWidget::HandleItemButtonClicked);
	}

	Super::NativeDestruct();
}

void UASLootEntryWidget::SetItemInstance(UASItemInstance* InItemInstance)
{
	if (ItemInstance != InItemInstance) bPressed = false;
	ItemInstance = InItemInstance;
	UASItemSlotViewModel* Model = NewObject<UASItemSlotViewModel>(this);
	Model->SetStandaloneItem(InItemInstance);
	SetViewModel(Model);
}

void UASLootEntryWidget::SetLootSource(UASLootWidget* InLootWidget)
{
	LootSource = InLootWidget;
}

FReply UASLootEntryWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !LootSource.IsValid()
		|| !ViewModel || !ViewModel->Presentation.bHasItem)
		return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
	// Intercept before the child button captures the mouse, preserving click-on-release below.
	bPressed = true;
	PressedItem = ViewModel->Presentation.ItemId;
	return FReply::Handled().CaptureMouse(TakeWidget()).DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

FReply UASLootEntryWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !bPressed)
		return Super::NativeOnMouseButtonUp(Geometry, Event);
	bPressed = false;
	if (ViewModel && ViewModel->Presentation.ItemId == PressedItem
		&& Geometry.IsUnderLocation(Event.GetScreenSpacePosition())) HandleItemButtonClicked();
	return FReply::Handled().ReleaseMouseCapture();
}

void UASLootEntryWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	bPressed = false;
	UASInventoryViewModel* InventoryModel = ViewModel ? ViewModel->GetInventoryViewModel() : nullptr;
	if (!LootSource.IsValid() || !LootSource->IsInViewport() || !InventoryModel || !InventoryModel->GetLootChest()
		|| !ViewModel->Presentation.bHasItem || ViewModel->Presentation.ItemId != PressedItem) return;
	UASLootItemDragDrop* Drag = NewObject<UASLootItemDragDrop>();
	Drag->SourceLoot = LootSource;
	Drag->Chest = InventoryModel->GetLootChest();
	Drag->ViewModel = InventoryModel;
	Drag->ItemId = PressedItem;
	UASLootEntryWidget* Visual = CreateWidget<UASLootEntryWidget>(GetOwningPlayer(), GetClass());
	if (!Visual) return;
	Visual->SetViewModel(ViewModel);
	Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
	Drag->DefaultDragVisual = Visual;
	Drag->Pivot = EDragPivot::CenterCenter;
	Operation = Drag;
}

void UASLootEntryWidget::SetViewModel(UASItemSlotViewModel* Model)
{
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	ViewModel = Model;
	bPressed = false;
	RefreshEntry();
}

void UASLootEntryWidget::RefreshEntry()
{
	if (!ViewModel && ItemInstance) { SetItemInstance(ItemInstance); return; }
	if (ViewModel)
	{
		ViewModel->RemoveAllFieldValueChangedDelegates(this);
		ViewModel->AddFieldValueChangedDelegate(UASItemSlotViewModel::FFieldNotificationClassDescriptor::Presentation,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this,
				[this](UObject*, UE::FieldNotification::FFieldId) { ApplyPresentation(); }));
	}
	ASItemViewBinding::Set(this, TEXT("SlotVM"), ViewModel);
	ApplyPresentation();
}

void UASLootEntryWidget::ApplyPresentation()
{
	const FASItemSlotPresentation P = ViewModel ? ViewModel->Presentation : FASItemSlotPresentation();
	const bool bAuthoredBindings = ASItemViewBinding::HasView(this);
	const ESlateVisibility QuantityVisibility = P.Quantity.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	if (QuantityText) { if (!bAuthoredBindings) QuantityText->SetText(P.Quantity); QuantityText->SetVisibility(QuantityVisibility); }
	if (RarityBackground) { RarityBackground->SetRarity(P.Rarity); RarityBackground->SetVisibility(P.bHasItem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (ItemNameText) { if (!bAuthoredBindings) ItemNameText->SetText(P.Name); ItemNameText->SetVisibility(!P.Name.IsEmpty() && !P.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (ItemButton) { ItemButton->SetToolTipText(P.Tooltip); ItemButton->SetIsEnabled(P.bHasItem); }
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(P.Icon, true);
		// Authored MVVM bindings may already have assigned the texture before this refresh.
		ItemIcon->SetDesiredSizeOverride(P.Icon ? FVector2D(P.Icon->GetSizeX(), P.Icon->GetSizeY()) : FVector2D::ZeroVector);
		ItemIcon->SetVisibility(P.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (QuantityBadge) QuantityBadge->SetVisibility(QuantityVisibility);
}

void UASLootEntryWidget::HandleItemButtonClicked()
{
	if (!ViewModel || !ViewModel->Presentation.bHasItem) return;
	// Retain the authored-entry event as a compatibility adapter. The native loot view uses the command.
	if (UASInventoryViewModel* Source = ViewModel->GetInventoryViewModel()) Source->LootItem(ViewModel->Presentation.ItemId);
	else OnItemClicked.Broadcast(ViewModel->ResolveItem());
}
