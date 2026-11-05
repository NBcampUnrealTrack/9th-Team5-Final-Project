#include "Items/UI/ASLootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Core/Framework/ASPlayerController.h"
#include "InputCoreTypes.h"
#include "Items/ASItemInstance.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ASLootEntryWidget.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Loot/ASLootChest.h"
#include "GameFramework/PlayerController.h"
#include "Relics/Data/ASRelicDefinition.h"

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
	LootPanel->SetPadding(FMargin(14.0f));
	LootPanel->SetBrushColor(FLinearColor(0.035f, 0.055f, 0.075f, 0.97f));
	UVerticalBox* RootColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootColumn"));
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Header"));
	LootTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LootTitleText"));
	LootTitleText->SetText(FText::FromString(TEXT("Loot Chest")));
	LootTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.91f, 0.98f, 1.0f)));
	FSlateFontInfo TitleFont = LootTitleText->GetFont();
	TitleFont.Size = 18;
	LootTitleText->SetFont(TitleFont);
	ItemCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemCountText"));
	ItemCountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.43f, 0.66f, 0.78f, 1.0f)));
	UButton* HeaderCloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("HeaderCloseButton"));
	HeaderCloseButton->SetBackgroundColor(FLinearColor(0.08f, 0.12f, 0.16f, 1.0f));
	HeaderCloseButton->OnClicked.AddUniqueDynamic(this, &UASLootWidget::HandleCloseClicked);
	UTextBlock* HeaderCloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderCloseText"));
	HeaderCloseText->SetText(FText::FromString(TEXT("X")));
	HeaderCloseButton->AddChild(HeaderCloseText);
	Header->AddChildToHorizontalBox(LootTitleText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Header->AddChildToHorizontalBox(ItemCountText)->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
	Header->AddChildToHorizontalBox(HeaderCloseButton);
	UTextBlock* SectionLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SectionLabel"));
	SectionLabel->SetText(FText::FromString(TEXT("CONTENTS")));
	SectionLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.42f, 0.53f, 0.61f, 1.0f)));
	UScrollBox* LootScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LootScrollBox"));
	LootScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	UUniformGridPanel* ItemGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ItemList"));
	ItemGrid->SetSlotPadding(FMargin(3.0f));
	ItemList = ItemGrid;
	EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EmptyText"));
	EmptyText->SetText(FText::FromString(TEXT("Empty")));
	EmptyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.47f, 0.56f, 0.62f, 1.0f)));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	UTextBlock* CloseButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseButtonText"));
	CloseButtonText->SetText(FText::FromString(TEXT("Close")));
	CloseButton->SetBackgroundColor(FLinearColor(0.10f, 0.17f, 0.22f, 1.0f));
	RelicClaimButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RelicClaimButton"));
	RelicClaimButton->SetBackgroundColor(FLinearColor(0.35f, 0.16f, 0.12f, 1.0f));
	RelicClaimText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RelicClaimText"));
	RelicClaimText->SetText(FText::FromString(TEXT("유물 획득")));
	RelicClaimText->SetAutoWrapText(true);
	RelicClaimButton->AddChild(RelicClaimText);
	RelicClaimButton->SetVisibility(ESlateVisibility::Collapsed);

	LootPanelSlot = RootCanvas->AddChildToCanvas(LootPanel);
	LootPanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	LootPanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	LootPanelSlot->SetPosition(FVector2D::ZeroVector);
	LootPanelSlot->SetSize(FVector2D(360.0f, 390.0f));
	LootPanel->AddChild(RootColumn);
	RootColumn->AddChildToVerticalBox(Header);
	RootColumn->AddChildToVerticalBox(SectionLabel)->SetPadding(FMargin(0.0f, 7.0f, 0.0f, 9.0f));
	UVerticalBoxSlot* ScrollSlot = RootColumn->AddChildToVerticalBox(LootScrollBox);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LootScrollBox->AddChild(ItemGrid);
	RootColumn->AddChildToVerticalBox(EmptyText)->SetPadding(FMargin(0.0f, 10.0f));
	RootColumn->AddChildToVerticalBox(RelicClaimButton)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	RootColumn->AddChildToVerticalBox(CloseButton)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
	CloseButton->AddChild(CloseButtonText);
	WidgetTree->RootWidget = RootCanvas;
}

void UASLootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWidget* DimBackground = IsValid(WidgetTree) ? WidgetTree->FindWidget(TEXT("DimBackground")) : nullptr)
	{
		DimBackground->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!IsValid(LootPanelSlot) && IsValid(WidgetTree))
	{
		if (UWidget* LootPanel = WidgetTree->FindWidget(TEXT("LootPanel")))
		{
			LootPanelSlot = Cast<UCanvasPanelSlot>(LootPanel->Slot);
			if (IsValid(LootPanelSlot))
			{
				LootPanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
				LootPanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			}
		}
	}

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UASLootWidget::HandleCloseClicked);
	}
	if (IsValid(RelicClaimButton))
	{
		RelicClaimButton->OnClicked.AddUniqueDynamic(this, &UASLootWidget::HandleRelicClaimClicked);
	}

	BindLootInventory();
	RefreshLootHeader();
	RefreshLoot();
}

void UASLootWidget::NativeDestruct()
{
	UnbindLootInventory();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UASLootWidget::HandleCloseClicked);
	}
	if (IsValid(RelicClaimButton))
	{
		RelicClaimButton->OnClicked.RemoveDynamic(this, &UASLootWidget::HandleRelicClaimClicked);
	}

	Super::NativeDestruct();
	OnLootClosed.Broadcast();
}

void UASLootWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsValid(LootPanelSlot) || !LootChest.IsValid() || !GetOwningPlayer())
	{
		return;
	}

	FVector2D ProjectedPosition;
	const FVector WorldAnchor = LootChest->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), WorldAnchor, ProjectedPosition, true))
	{
		return;
	}

	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	const FVector2D PanelSize = LootPanelSlot->GetSize();
	ProjectedPosition.X = FMath::Clamp(ProjectedPosition.X, PanelSize.X * 0.5f + 12.0f, ViewportSize.X - PanelSize.X * 0.5f - 12.0f);
	ProjectedPosition.Y = FMath::Clamp(ProjectedPosition.Y, PanelSize.Y + 12.0f, ViewportSize.Y - 12.0f);
	LootPanelSlot->SetPosition(ProjectedPosition);
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
			if (UASItemInteractionComponent* Interaction = PC->FindComponentByClass<UASItemInteractionComponent>())
			{
				Interaction->HandleWorldRightClick();
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
	LootChest = IsValid(InLootInventory) ? Cast<AASLootChest>(InLootInventory->GetOwner()) : nullptr;
	BindLootInventory();
	RefreshLootHeader();
	RefreshLoot();
}

bool UASLootWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	UASLootItemDragDrop* Drag = Cast<UASLootItemDragDrop>(Operation);
	if (!Drag || Drag->SourceLoot.Get() != this) return Super::NativeOnDrop(Geometry, Event, Operation);
	UWidget* Panel = WidgetTree ? WidgetTree->FindWidget(TEXT("LootPanel")) : nullptr;
	// Gaps, empty slots, or the source entry inside the chest are never discard targets.
	if (!IsInViewport() || !IsVisible() || !Panel || Drag->Chest != LootChest
		|| Panel->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition())) return true;
	APlayerController* PC = GetOwningPlayer();
	UASItemInteractionComponent* Interaction = PC ? PC->FindComponentByClass<UASItemInteractionComponent>() : nullptr;
	if (Interaction) Interaction->RequestDropLoot(Drag->Chest.Get(), Drag->ItemId);
	return true;
}

void UASLootWidget::RefreshLootHeader()
{
	if (IsValid(LootTitleText))
	{
		FLinearColor Accent(0.35f, 0.73f, 0.92f, 1.0f);
		FText Title = FText::FromString(TEXT("Loot Chest"));
		if (IsValid(LootInventory))
		{
			if (const AASLootChest* Chest = Cast<AASLootChest>(LootInventory->GetOwner()))
			{
				switch (Chest->GetLootRarity())
				{
				case EASLootRarity::Uncommon: Title = FText::FromString(TEXT("고급 보급 상자")); Accent = FLinearColor(0.35f, 0.78f, 0.46f, 1.0f); break;
				case EASLootRarity::Rare: Title = FText::FromString(TEXT("레어 보급 상자")); Accent = FLinearColor(0.68f, 0.49f, 0.91f, 1.0f); break;
				case EASLootRarity::Epic: Title = FText::FromString(TEXT("에픽 보급 상자")); Accent = FLinearColor(0.72f, 0.39f, 0.91f, 1.0f); break;
				case EASLootRarity::Legendary: Title = FText::FromString(TEXT("전설 보급 상자")); Accent = FLinearColor(0.96f, 0.60f, 0.28f, 1.0f); break;
				default: break;
				}
				// 표시 이름이 있으면 희귀도 문구 대신 사용 (시체 상자 등)
				if (!Chest->GetLootDisplayName().IsEmpty())
				{
					Title = Chest->GetLootDisplayName();
				}
			}
		}
		LootTitleText->SetText(Title);
		LootTitleText->SetColorAndOpacity(FSlateColor(Accent));
	}
}

void UASLootWidget::RefreshLoot()
{
	if (!IsValid(ItemList))
	{
		return;
	}

	ItemList->ClearChildren();
	RefreshRelicRewardButton();

	if (!IsValid(LootInventory))
	{
		if (IsValid(EmptyText)) EmptyText->SetVisibility(ESlateVisibility::Visible);
		if (IsValid(ItemCountText)) ItemCountText->SetText(FText::FromString(TEXT("0")));
		return;
	}
	const TArray<UASItemInstance*> Items = LootInventory->GetItems();
	if (IsValid(ItemCountText)) ItemCountText->SetText(FText::AsNumber(Items.Num()));
	if (IsValid(EmptyText)) EmptyText->SetVisibility(Items.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const int32 ConfiguredSlotCount = LootInventory->GetMaxSlots();
	const int32 VisibleSlotCount = ConfiguredSlotCount > 0
		? ConfiguredSlotCount
		: FMath::Max(12, FMath::DivideAndRoundUp(Items.Num(), 4) * 4);

	TSubclassOf<UASLootEntryWidget> ActualEntryClass = EntryWidgetClass;
	if (!ActualEntryClass)
	{
		ActualEntryClass = UASLootEntryWidget::StaticClass();
	}

	for (UASItemInstance* ItemInstance : Items)
	{
		UASLootEntryWidget* EntryWidget = CreateWidget<UASLootEntryWidget>(GetOwningPlayer(), ActualEntryClass);
		if (!IsValid(EntryWidget))
		{
			continue;
		}

		EntryWidget->SetItemInstance(ItemInstance);
		EntryWidget->SetLootSource(this);
		EntryWidget->OnItemClicked.AddUniqueDynamic(this, &UASLootWidget::HandleItemClicked);
		if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(ItemList))
		{
			const int32 SlotIndex = ItemInstance->GetInventorySlot();
			UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(EntryWidget, SlotIndex / 4, SlotIndex % 4);
			if (GridSlot)
			{
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
		else ItemList->AddChild(EntryWidget);
	}

	if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(ItemList))
	{
		for (int32 SlotIndex = 0; SlotIndex < VisibleSlotCount; ++SlotIndex)
		{
			if (LootInventory->GetItemAtSlot(SlotIndex))
			{
				continue;
			}

			USizeBox* EmptySlot = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			EmptySlot->SetWidthOverride(80.0f);
			EmptySlot->SetHeightOverride(80.0f);
			UBorder* SlotBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			SlotBackground->SetBrushColor(FLinearColor(0.045f, 0.055f, 0.065f, 0.72f));
			EmptySlot->AddChild(SlotBackground);
			if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(EmptySlot, SlotIndex / 4, SlotIndex % 4))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
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

void UASLootWidget::RefreshRelicRewardButton()
{
	if (!IsValid(RelicClaimButton)) return;
	const AASLootChest* Chest = LootChest.Get();
	if (!IsValid(Chest) || !Chest->HasUnclaimedRelicReward())
	{
		RelicClaimButton->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	RelicClaimButton->SetVisibility(ESlateVisibility::Visible);
	if (IsValid(RelicClaimText))
	{
		const UASRelicDefinition* Relic = Chest->GetRelicReward();
		FText Label = FText::FromString(TEXT("유물 획득"));
		if (Relic && !Relic->DisplayName.IsEmpty() && !Relic->Description.IsEmpty())
		{
			Label = FText::Format(FText::FromString(TEXT("{0}\n{1}\n[획득]")),
				Relic->DisplayName, Relic->Description);
		}
		else if (Relic && !Relic->DisplayName.IsEmpty())
		{
			Label = FText::Format(FText::FromString(TEXT("유물 획득: {0}")), Relic->DisplayName);
		}
		RelicClaimText->SetText(Label);
	}
}

void UASLootWidget::HandleCloseClicked()
{
	RemoveFromParent();
}

void UASLootWidget::HandleRelicClaimClicked()
{
	APlayerController* PC = GetOwningPlayer();
	UASItemInteractionComponent* Interaction = PC ? PC->FindComponentByClass<UASItemInteractionComponent>() : nullptr;
	if (Interaction && LootChest.IsValid())
	{
		Interaction->RequestClaimRelic(LootChest.Get());
		Interaction->CloseLoot();
	}
}
