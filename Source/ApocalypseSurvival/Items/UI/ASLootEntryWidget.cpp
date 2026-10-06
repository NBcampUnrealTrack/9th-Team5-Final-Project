#include "Items/UI/ASLootEntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"

TSharedRef<SWidget> UASLootEntryWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UASLootEntryWidget::BuildDefaultWidgetTree()
{
	if (!IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	USizeBox* EntrySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EntrySize"));
	EntrySize->SetWidthOverride(360.0f);
	EntrySize->SetHeightOverride(72.0f);

	ItemButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ItemButton"));
	UHorizontalBox* EntryRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EntryRow"));
	ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
	ItemIcon->SetDesiredSizeOverride(FVector2D(56.0f, 56.0f));
	UVerticalBox* TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextColumn"));
	ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemNameText"));
	QuantityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuantityText"));

	ItemNameText->SetText(FText::FromString(TEXT("Item Name")));
	QuantityText->SetText(FText::AsNumber(1));

	EntrySize->AddChild(ItemButton);
	ItemButton->AddChild(EntryRow);
	EntryRow->AddChildToHorizontalBox(ItemIcon);
	EntryRow->AddChildToHorizontalBox(TextColumn);
	TextColumn->AddChildToVerticalBox(ItemNameText);
	TextColumn->AddChildToVerticalBox(QuantityText);
	WidgetTree->RootWidget = EntrySize;
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
	if (IsValid(ItemButton))
	{
		ItemButton->OnClicked.RemoveDynamic(this, &UASLootEntryWidget::HandleItemButtonClicked);
	}

	Super::NativeDestruct();
}

void UASLootEntryWidget::SetItemInstance(UASItemInstance* InItemInstance)
{
	ItemInstance = InItemInstance;
	RefreshEntry();
}

void UASLootEntryWidget::RefreshEntry()
{
	if (!IsValid(ItemInstance) || !IsValid(ItemInstance->GetItemDefinition()))
	{
		return;
	}

	UASItemDefinition* ItemDefinition = ItemInstance->GetItemDefinition();

	if (IsValid(ItemNameText))
	{
		ItemNameText->SetText(ItemDefinition->DisplayName);
	}

	if (IsValid(QuantityText))
	{
		QuantityText->SetText(FText::AsNumber(ItemInstance->GetQuantity()));
	}

	if (IsValid(ItemIcon))
	{
		ItemIcon->SetBrushFromTexture(ItemDefinition->Icon.LoadSynchronous());
	}
}

void UASLootEntryWidget::HandleItemButtonClicked()
{
	if (IsValid(ItemInstance) && ItemInstance->IsValidItemInstance())
	{
		OnItemClicked.Broadcast(ItemInstance);
	}
}
