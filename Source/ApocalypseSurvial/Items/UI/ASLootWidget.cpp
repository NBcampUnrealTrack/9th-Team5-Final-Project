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
#include "Items/ASItemInstance.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ASLootEntryWidget.h"

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

	LootInventory->TransferItemTo(PlayerInventory, ItemInstance, ItemInstance->GetQuantity());
}

void UASLootWidget::HandleCloseClicked()
{
	RemoveFromParent();
}
