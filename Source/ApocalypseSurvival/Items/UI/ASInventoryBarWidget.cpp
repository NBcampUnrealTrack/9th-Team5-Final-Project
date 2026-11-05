#include "Items/UI/ASInventoryBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/UniformGridPanel.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ASInventorySlotWidget.h"
#include "Items/UI/ViewModels/ASInventoryViewModel.h"
#include "Items/UI/ASItemViewBinding.h"

UASInventoryBarWidget::UASInventoryBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultSlotWidgetClass = FSoftObjectPath(TEXT("/Game/ApocalypseSurvival/Items/UI/WBP_InventorySlot.WBP_InventorySlot_C"));
}

void UASInventoryBarWidget::NativePreConstruct()
{
	if (IsDesignTime()) BuildSlotWidgets();
	Super::NativePreConstruct();
}

UASInventorySlotWidget* UASInventoryBarWidget::CreateSlotWidget(int32 Index)
{
	const TSubclassOf<UASInventorySlotWidget> Class = SlotWidgetClass ? SlotWidgetClass.Get() : DefaultSlotWidgetClass.LoadSynchronous();
	if (!Class) return nullptr;
	UASInventorySlotWidget* Entry = nullptr;
	if (IsDesignTime())
	{
		Entry = NewObject<UASInventorySlotWidget>(WidgetTree, Class);
		Entry->SetDesignerFlags(GetDesignerFlags());
		Entry->Initialize();
	}
	else Entry = CreateWidget<UASInventorySlotWidget>(GetOwningPlayer(), Class);
	if (Entry) { Entry->InitializeSlot(this, Index); Slots.Add(Entry); }
	return Entry;
}

void UASInventoryBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BuildSlotWidgets();
	BindViewModel();
	RefreshSlots();
}

void UASInventoryBarWidget::BuildSlotWidgets()
{
	if (SlotsPanel)
	{
		SlotsPanel->ClearChildren();
		Slots.Reset();
		for (int32 Index = -1; Index < UASInventoryComponent::PlayerSlotCount; ++Index)
		{
			UASInventorySlotWidget* Entry = CreateSlotWidget(Index < 0 ? UASInventoryComponent::WeaponSlotIndex : Index);
			if (!Entry) continue;
			SlotsPanel->AddChildToUniformGrid(Entry, Index < 0 ? 0 : Index / FMath::Max(1, QuickSlotsPerRow), Index < 0 ? 0 : Index % FMath::Max(1, QuickSlotsPerRow) + 1);
		}
	}
}

UWidget* UASInventoryBarWidget::GetInventoryBounds() const
{
	return SlotsPanel;
}

void UASInventoryBarWidget::NativeDestruct()
{
	EndItemDrag();
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
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
	if (!Drag || Drag->SourceBar != this || Drag->ViewModel != ViewModel || !ViewModel) return false;
	// Dropping in gaps or back onto the source slot is a no-op, not a discard.
	UWidget* Bounds = GetInventoryBounds();
	if (!Bounds || Bounds->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition())) return true;
	if (ViewModel) ViewModel->DropSlot(Drag->FromSlot, Drag->ItemId);
	return true;
}

bool UASInventoryBarWidget::IsCursorOverInventory() const
{
	UWidget* Bounds = GetInventoryBounds();
	return IsInViewport() && IsVisible() && Bounds && Bounds->IsVisible()
		&& FSlateApplication::IsInitialized()
		&& Bounds->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void UASInventoryBarWidget::SetInventory(UASInventoryComponent* InInventory)
{
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	Inventory = InInventory;
	UASItemInteractionComponent* Interaction = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<UASItemInteractionComponent>() : nullptr;
	if (Interaction && Interaction->GetPlayerInventory() == InInventory) ViewModel = Interaction->GetInventoryViewModel();
	else
	{
		ViewModel = NewObject<UASInventoryViewModel>(this);
		ViewModel->Initialize(InInventory, GetOwningPlayer());
	}
	BindViewModel();
	RefreshSlots();
}

void UASInventoryBarWidget::RefreshSlots()
{
	for (UASInventorySlotWidget* Entry : Slots) if (IsValid(Entry)) Entry->RefreshSlot();
}

void UASInventoryBarWidget::ActivateSlot(const int32 SlotIndex, const FGuid ExpectedItem)
{
	if (ViewModel) ViewModel->ActivateSlot(SlotIndex, ExpectedItem);
}

void UASInventoryBarWidget::DropOneFromSlot(const int32 SlotIndex, const FGuid ExpectedItem)
{
	if (ViewModel) ViewModel->DropSlot(SlotIndex, ExpectedItem, true);
}

void UASInventoryBarWidget::SwapSlots(const int32 FromSlot, const int32 ToSlot,
	const FGuid ExpectedFrom, const FGuid ExpectedTo)
{
	if (ViewModel) ViewModel->MoveSlot(FromSlot, ToSlot, ExpectedFrom, ExpectedTo);
}

void UASInventoryBarWidget::BindViewModel()
{
	ASItemViewBinding::Set(this, TEXT("InventoryVM"), ViewModel);
	if (!ViewModel) return;
	ViewModel->RemoveAllFieldValueChangedDelegates(this);
	using FFields = UASInventoryViewModel::FFieldNotificationClassDescriptor;
	for (UE::FieldNotification::FFieldId Field : {FFields::Presentation, FFields::QuickSlots, FFields::StorageSlots})
		ViewModel->AddFieldValueChangedDelegate(Field,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this,
				[this](UObject*, UE::FieldNotification::FFieldId) { RefreshSlots(); }));
}
