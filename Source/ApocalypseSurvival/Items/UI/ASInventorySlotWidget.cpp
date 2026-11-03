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
#include "Items/UI/ASItemRarityBackground.h"

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
		SlotBorder->SetPadding(FMargin(1.0f));
		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
		Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RarityBackground = WidgetTree->ConstructWidget<UASItemRarityBackground>(UASItemRarityBackground::StaticClass(), TEXT("RarityBackground"));
		RarityBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
		Overlay->AddChildToOverlay(RarityBackground);
		ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
		ItemIcon->SetDesiredSizeOverride(FVector2D(70.0f));
		UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(ItemIcon);
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
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
	// Support authored slot layouts that already contain an icon overlay.
	if (WidgetTree && !RarityBackground && ItemIcon)
	{
		if (UOverlay* Overlay = Cast<UOverlay>(ItemIcon->GetParent()))
		{
			RarityBackground = WidgetTree->ConstructWidget<UASItemRarityBackground>(UASItemRarityBackground::StaticClass(), TEXT("RarityBackground"));
			RarityBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
			Overlay->InsertChildAt(0, RarityBackground);
		}
	}
	for (UWidget* Layer : { static_cast<UWidget*>(RarityBackground.Get()), static_cast<UWidget*>(ItemIcon.Get()) })
	{
		if (UOverlaySlot* LayerSlot = Layer ? Cast<UOverlaySlot>(Layer->Slot) : nullptr)
		{
			LayerSlot->SetPadding(FMargin(0.0f));
			LayerSlot->SetHorizontalAlignment(HAlign_Fill);
			LayerSlot->SetVerticalAlignment(VAlign_Fill);
		}
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

void UASInventorySlotWidget::NativeDestruct()
{
	IconAssetRequest.Reset();
	Super::NativeDestruct();
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
	IconAssetRequest.Request(Definition ? Definition->Icon.ToSoftObjectPath() : FSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this] { RefreshSlot(); }));
	UTexture2D* Icon = Definition ? Definition->Icon.Get() : nullptr;
	const bool bWeaponSlot = SlotIndex == UASInventoryComponent::WeaponSlotIndex;
	const bool bBagSlot = SlotIndex == UASInventoryComponent::BagSlotIndex;
	const bool bQuickSlot = SlotIndex >= 0 && SlotIndex < UASInventoryComponent::QuickSlotCount;
	const bool bEquipped = Item && (bWeaponSlot || bBagSlot);
	if (RarityBackground)
	{
		if (Definition) RarityBackground->SetRarity(Definition->Rarity);
		RarityBackground->SetVisibility(Definition ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (KeyText) KeyText->SetText(bWeaponSlot ? NSLOCTEXT("Items", "WeaponSlot", "무기")
		: bBagSlot ? NSLOCTEXT("Items", "BagSlot", "가방")
		: bQuickSlot ? FText::AsNumber((SlotIndex + 1) % 10) : FText::GetEmpty());
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(Icon);
		ItemIcon->SetDesiredSizeOverride(FVector2D(70.0f));
		ItemIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ItemNameText) ItemNameText->SetText(Definition && !Icon ? Definition->DisplayName : FText::GetEmpty());
	if (QuantityText) QuantityText->SetText(Item && Item->GetQuantity() > 1 ? FText::AsNumber(Item->GetQuantity()) : FText::GetEmpty());
	if (EquippedText) EquippedText->SetText(bEquipped ? FText::FromString(TEXT("E")) : FText::GetEmpty());
	if (SlotBorder) SlotBorder->SetBrushColor(bEquipped ? FLinearColor(0.15f, 0.4f, 0.65f, 0.95f) : FLinearColor(0.08f, 0.1f, 0.13f, 0.95f));
	SetToolTipText(Definition && Definition->ItemType == EASItemType::Bag
		? FText::Format(NSLOCTEXT("Items", "BagCapacityTooltip", "{0}\n장착 시 보관함 +{1}칸"),
			Definition->DisplayName, FText::AsNumber(Definition->BagAdditionalSlots))
		: Definition ? Definition->DisplayName : FText::GetEmpty());
}

FReply UASInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (Event.IsControlDown() && Bar && SlotIndex >= 0)
		{
			if (UASItemInstance* Item = GetItem())
				Bar->DropOneFromSlot(SlotIndex, Item->GetInstanceId());
		}
		// All right clicks over slots belong to the UI, including empty and weapon slots.
		return FReply::Handled();
	}
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return Super::NativeOnMouseButtonDown(Geometry, Event);
	bPressed = true;
	PressedItem = GetItem() ? GetItem()->GetInstanceId() : FGuid();
	FReply Reply = FReply::Handled().CaptureMouse(TakeWidget());
	return PressedItem.IsValid() && (SlotIndex >= 0 || SlotIndex == UASInventoryComponent::BagSlotIndex) ? Reply.DetectDrag(TakeWidget(), EKeys::LeftMouseButton) : Reply;
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
	UASItemInstance* Item = GetItem();
	if (!Bar || !Item || (SlotIndex < 0 && SlotIndex != UASInventoryComponent::BagSlotIndex) || Item->GetInstanceId() != PressedItem) return;
	UASInventorySlotDragDrop* Drag = NewObject<UASInventorySlotDragDrop>();
	Drag->Inventory = Bar->GetInventory();
	Drag->SourceBar = Bar;
	Drag->FromSlot = SlotIndex;
	Drag->ItemId = PressedItem;
	UImage* Visual = NewObject<UImage>(Drag);
	UTexture2D* Icon = Item->GetItemDefinition()->Icon.Get();
	Visual->SetBrushFromTexture(Icon);
	Visual->SetDesiredSizeOverride(FVector2D(56.0f));
	Visual->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	USizeBox* DragSize = NewObject<USizeBox>(Drag);
	DragSize->SetWidthOverride(56.0f);
	DragSize->SetHeightOverride(56.0f);
	DragSize->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlay* DragOverlay = NewObject<UOverlay>(Drag);
	UASItemRarityBackground* DragBackground = NewObject<UASItemRarityBackground>(Drag);
	DragBackground->SetRarity(Item->GetItemDefinition()->Rarity);
	UOverlaySlot* DragBackgroundSlot = DragOverlay->AddChildToOverlay(DragBackground);
	DragBackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	DragBackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	UOverlaySlot* DragIconSlot = DragOverlay->AddChildToOverlay(Visual);
	DragIconSlot->SetHorizontalAlignment(HAlign_Fill);
	DragIconSlot->SetVerticalAlignment(VAlign_Fill);
	if (!Icon)
	{
		UTextBlock* LoadingName = NewObject<UTextBlock>(Drag);
		LoadingName->SetText(Item->GetItemDefinition()->DisplayName);
		LoadingName->SetVisibility(ESlateVisibility::HitTestInvisible);
		DragOverlay->AddChildToOverlay(LoadingName);
	}
	DragSize->SetContent(DragOverlay);
	Drag->DefaultDragVisual = DragSize;
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
	if (SlotIndex == UASInventoryComponent::WeaponSlotIndex)
	{
		UASItemInstance* Source = Bar->GetInventory()->GetItemAtSlot(Drag->FromSlot);
		if (Source && Source->GetInstanceId() == Drag->ItemId && Source->GetItemDefinition()->ItemType == EASItemType::Weapon)
			Bar->ActivateSlot(Drag->FromSlot, Drag->ItemId);
		return true;
	}
	Bar->SwapSlots(Drag->FromSlot, SlotIndex, Drag->ItemId, GetItem() ? GetItem()->GetInstanceId() : FGuid());
	return true;
}
