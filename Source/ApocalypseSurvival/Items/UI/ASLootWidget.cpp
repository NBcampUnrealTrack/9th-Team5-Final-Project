#include "Items/UI/ASLootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/Framework/ASPlayerController.h"
#include "InputCoreTypes.h"
#include "Items/ASItemInstance.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ASLootEntryWidget.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Loot/ASLootChest.h"
#include "GameFramework/PlayerController.h"

TSharedRef<SWidget> UASLootWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UASLootWidget::BuildDefaultWidgetTree()
{
	if (!IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	OutsideCloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OutsideCloseButton"));
	OutsideCloseButton->SetRenderOpacity(0.0f);
	UCanvasPanelSlot* OutsideButtonSlot = RootCanvas->AddChildToCanvas(OutsideCloseButton);
	OutsideButtonSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	OutsideButtonSlot->SetOffsets(FMargin(0.0f));
	OutsideButtonSlot->SetZOrder(-1);

	UBorder* LootPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LootPanel"));
	LootPanel->SetPadding(FMargin(16.0f));
	UVerticalBox* RootColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootColumn"));
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("Loot")));
	UScrollBox* LootScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LootScrollBox"));
	UVerticalBox* ItemListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemList"));
	ItemList = ItemListBox;
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	UTextBlock* CloseButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseButtonText"));
	CloseButtonText->SetText(FText::FromString(TEXT("Close")));

	UCanvasPanelSlot* LootPanelSlot = RootCanvas->AddChildToCanvas(LootPanel);
	LootPanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	LootPanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	LootPanelSlot->SetSize(FVector2D(420.0f, 500.0f));
	LootPanel->AddChild(RootColumn);
	RootColumn->AddChildToVerticalBox(TitleText);
	UVerticalBoxSlot* ScrollSlot = RootColumn->AddChildToVerticalBox(LootScrollBox);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LootScrollBox->AddChild(ItemListBox);
	RootColumn->AddChildToVerticalBox(CloseButton);
	CloseButton->AddChild(CloseButtonText);
	WidgetTree->RootWidget = RootCanvas;
}

void UASLootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UASLootWidget::HandleCloseClicked);
	}

	BindLootInventory();
	RefreshLoot();
}

void UASLootWidget::NativeDestruct()
{
	UnbindLootInventory();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UASLootWidget::HandleCloseClicked);
	}

	Super::NativeDestruct();
	OnLootClosed.Broadcast();
}

FReply UASLootWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	UWidget* LootPanel = IsValid(WidgetTree) ? WidgetTree->FindWidget(TEXT("LootPanel")) : nullptr;
	if (!IsValid(LootPanel))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	// Right clicks inside the panel stay in the UI. Outside clicks only issue movement;
	// the interaction component closes the widget once the pawn actually leaves range.
	if (!LootPanel->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		if (AASPlayerController* PC = Cast<AASPlayerController>(GetOwningPlayer()))
		{
			FVector Destination;
			if (PC->GetCursorWorldLocation(Destination))
			{
				PC->RequestMoveTo(Destination);
			}
		}
	}
	return FReply::Handled();
}

void UASLootWidget::SetLootContext(
	UASInventoryComponent* InLootInventory,
	UASInventoryComponent* InPlayerInventory)
{
	UnbindLootInventory();
	LootInventory = InLootInventory;
	PlayerInventory = InPlayerInventory;
	BindLootInventory();
	RefreshLoot();
}

void UASLootWidget::RefreshLoot()
{
	if (!IsValid(ItemList))
	{
		return;
	}

	ItemList->ClearChildren();

	if (!IsValid(LootInventory))
	{
		return;
	}

	TSubclassOf<UASLootEntryWidget> ActualEntryClass = EntryWidgetClass;
	if (!ActualEntryClass)
	{
		ActualEntryClass = UASLootEntryWidget::StaticClass();
	}

	for (UASItemInstance* ItemInstance : LootInventory->GetItems())
	{
		UASLootEntryWidget* EntryWidget = CreateWidget<UASLootEntryWidget>(GetOwningPlayer(), ActualEntryClass);
		if (!IsValid(EntryWidget))
		{
			continue;
		}

		EntryWidget->SetItemInstance(ItemInstance);
		EntryWidget->OnItemClicked.AddUniqueDynamic(this, &UASLootWidget::HandleItemClicked);
		ItemList->AddChild(EntryWidget);
	}
}

void UASLootWidget::BindLootInventory()
{
	if (IsValid(LootInventory))
	{
		LootInventory->OnInventoryChanged.AddUniqueDynamic(this, &UASLootWidget::HandleInventoryChanged);
	}
}

void UASLootWidget::UnbindLootInventory()
{
	if (IsValid(LootInventory))
	{
		LootInventory->OnInventoryChanged.RemoveDynamic(this, &UASLootWidget::HandleInventoryChanged);
	}
}

void UASLootWidget::HandleInventoryChanged()
{
	RefreshLoot();
}

void UASLootWidget::HandleItemClicked(UASItemInstance* ItemInstance)
{
	if (!IsValid(LootInventory) || !IsValid(PlayerInventory) || !IsValid(ItemInstance))
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	AASLootChest* Chest = Cast<AASLootChest>(LootInventory->GetOwner());
	UASItemInteractionComponent* Interaction = PC ? PC->FindComponentByClass<UASItemInteractionComponent>() : nullptr;
	if (Interaction && IsValid(Chest))
	{
		Interaction->RequestLoot(Chest, ItemInstance->GetInstanceId(), ItemInstance->GetQuantity());
	}
}

void UASLootWidget::HandleCloseClicked()
{
	RemoveFromParent();
}
