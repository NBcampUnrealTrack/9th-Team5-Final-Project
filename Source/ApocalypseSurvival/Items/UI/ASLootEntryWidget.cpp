#include "Items/UI/ASLootEntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
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
	EntrySize->SetWidthOverride(80.0f);
	EntrySize->SetHeightOverride(80.0f);

	ItemButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ItemButton"));
	ItemButton->SetBackgroundColor(FLinearColor(0.045f, 0.055f, 0.065f, 1.0f));
	UOverlay* EntryOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("EntryOverlay"));
	ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
	ItemIcon->SetDesiredSizeOverride(FVector2D(72.0f, 72.0f));
	ItemIcon->SetColorAndOpacity(FLinearColor::White);
	ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemNameText"));
	QuantityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuantityText"));
	ItemNameText->SetVisibility(ESlateVisibility::Collapsed);
	QuantityText->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.96f, 0.98f, 1.0f)));
	UBorder* QuantityBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuantityBadge"));
	QuantityBadge->SetPadding(FMargin(4.0f, 1.0f));
	QuantityBadge->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.04f, 0.88f));
	QuantityBadge->AddChild(QuantityText);

	EntrySize->AddChild(ItemButton);
	ItemButton->AddChild(EntryOverlay);
	EntryOverlay->AddChildToOverlay(ItemIcon);
	UOverlaySlot* BadgeSlot = EntryOverlay->AddChildToOverlay(QuantityBadge);
	BadgeSlot->SetHorizontalAlignment(HAlign_Right);
	BadgeSlot->SetVerticalAlignment(VAlign_Bottom);
	BadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 3.0f, 3.0f));
	EntryOverlay->AddChildToOverlay(ItemNameText);
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
	if (IsValid(WidgetTree))
	{
		if (USizeBox* EntrySize = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("EntrySize"))))
		{
			EntrySize->SetWidthOverride(80.0f);
			EntrySize->SetHeightOverride(80.0f);
		}
	}
	if (IsValid(ItemIcon))
	{
		ItemIcon->SetDesiredSizeOverride(FVector2D(72.0f, 72.0f));
		ItemIcon->SetColorAndOpacity(FLinearColor::White);
	}

	if (!IsValid(ItemInstance) || !IsValid(ItemInstance->GetItemDefinition()))
	{
		return;
	}

	UASItemDefinition* ItemDefinition = ItemInstance->GetItemDefinition();

	if (IsValid(ItemNameText))
	{
		ItemNameText->SetText(ItemDefinition->DisplayName);
	}
	if (IsValid(ItemButton)) ItemButton->SetToolTipText(ItemDefinition->DisplayName);

	if (IsValid(QuantityText))
	{
		const int32 Quantity = ItemInstance->GetQuantity();
		QuantityText->SetText(Quantity > 1
			? FText::FromString(FString::Printf(TEXT("x%d"), Quantity))
			: FText::GetEmpty());
	}
	if (IsValid(ItemButton))
	{
		ItemButton->SetBackgroundColor(FLinearColor(0.045f, 0.055f, 0.065f, 1.0f));
	}

	if (IsValid(ItemIcon))
	{
		ItemIcon->SetBrushFromTexture(ItemDefinition->Icon.LoadSynchronous(), false);
		ItemIcon->SetColorAndOpacity(FLinearColor::White);
	}
}

void UASLootEntryWidget::HandleItemButtonClicked()
{
	if (IsValid(ItemInstance) && ItemInstance->IsValidItemInstance())
	{
		OnItemClicked.Broadcast(ItemInstance);
	}
}
