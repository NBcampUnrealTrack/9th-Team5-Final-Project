#include "Items/UI/ASLootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/SizeBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Core/Framework/ASPlayerController.h"
#include "InputCoreTypes.h"
#include "Items/UI/ViewModels/ASInventoryViewModel.h"
#include "Items/UI/ViewModels/ASItemSlotViewModel.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/UI/ASLootEntryWidget.h"
#include "Items/UI/ASItemViewBinding.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Loot/ASLootChest.h"
#include "GameFramework/PlayerController.h"
#include "Relics/Data/ASRelicDefinition.h"

namespace
{
	constexpr float LootTileSize = 72.f;
	constexpr float LootTileSpacing = 6.f;
}

UASLootWidget::UASLootWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultEntryWidgetClass = FSoftObjectPath(TEXT("/Game/ApocalypseSurvival/Items/UI/WBP_LootEntry.WBP_LootEntry_C"));
}

TSharedRef<SWidget> UASLootWidget::RebuildWidget()
{
	if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(ItemList))
	{
		if (UScrollBox* Scroll = Cast<UScrollBox>(Grid->GetParent()))
		{
			const FMargin ListPadding = CastChecked<UScrollBoxSlot>(Grid->Slot)->GetPadding();
			Grid->RemoveFromParent();
			// UniformGrid divides all allotted space between rows/columns. Fixed tiles instead
			// wrap without distributing leftover space into the cards or their gaps.
			UWrapBox* Tiles = WidgetTree->ConstructWidget<UWrapBox>();
			Tiles->SetExplicitWrapSize(false);
			Tiles->SetInnerSlotPadding(FVector2D(LootTileSpacing));
			Tiles->SetHorizontalAlignment(HAlign_Left);
			USizeBox* ListWidth = WidgetTree->ConstructWidget<USizeBox>();
			const int32 MaxColumns = FMath::Max(1, Columns);
			ListWidth->SetWidthOverride(MaxColumns * LootTileSize + (MaxColumns - 1) * LootTileSpacing);
			ListWidth->SetContent(Tiles);
			UScrollBoxSlot* ListSlot = CastChecked<UScrollBoxSlot>(Scroll->AddChild(ListWidth));
			ListSlot->SetPadding(ListPadding);
			ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ListSlot->SetHorizontalAlignment(HAlign_Left);
			ListSlot->SetVerticalAlignment(VAlign_Top);
			ItemList = Tiles;
			// SetLootContext may have already populated the authored grid.
			RenderedSlotCount = INDEX_NONE;
		}
	}
	return Super::RebuildWidget();
}

void UASLootWidget::NativePreConstruct()
{
	if (IsDesignTime() && ItemList)
	{
		ItemList->ClearChildren();
		const TSubclassOf<UASLootEntryWidget> Class = EntryWidgetClass ? EntryWidgetClass.Get() : DefaultEntryWidgetClass.LoadSynchronous();
		if (Class) for (int32 Index = 0; Index < 12; ++Index)
		{
			UASLootEntryWidget* Entry = NewObject<UASLootEntryWidget>(WidgetTree, Class);
			Entry->SetDesignerFlags(GetDesignerFlags()); Entry->Initialize();
			if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(ItemList)) Grid->AddChildToUniformGrid(Entry, Index / FMath::Max(1, Columns), Index % FMath::Max(1, Columns));
			else if (UWrapBox* Tiles = Cast<UWrapBox>(ItemList))
			{
				USizeBox* Tile = WidgetTree->ConstructWidget<USizeBox>();
				Tile->SetWidthOverride(LootTileSize); Tile->SetHeightOverride(LootTileSize); Tile->SetContent(Entry);
				Tiles->AddChildToWrapBox(Tile);
			}
			else ItemList->AddChild(Entry);
		}
	}
	Super::NativePreConstruct();
}

void UASLootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Keep wheel input in the loot list, including at either end of the scroll range.
	for (UWidget* Parent = ItemList ? ItemList->GetParent() : nullptr; Parent; Parent = Parent->GetParent())
	{
		if (UScrollBox* Scroll = Cast<UScrollBox>(Parent)) Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	}
	if (UScrollBoxSlot* ListSlot = ItemList ? Cast<UScrollBoxSlot>(ItemList->Slot) : nullptr)
	{
		// Scroll the grid at its desired row height instead of stretching rows to the viewport.
		ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ListSlot->SetVerticalAlignment(VAlign_Top);
	}

	if (UWidget* DimBackground = IsValid(WidgetTree) ? WidgetTree->FindWidget(TEXT("DimBackground")) : nullptr)
	{
		DimBackground->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!IsValid(LootPanelSlot) && IsValid(WidgetTree))
	{
		if (UWidget* LootPanel = WidgetTree->FindWidget(TEXT("LootPanel")))
		{
			LootPanelSlot = Cast<UCanvasPanelSlot>(LootPanel->Slot);
		}
	}

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UASLootWidget::HandleCloseClicked);
	}
	// Older loot widget Blueprints do not have a relic button yet, so add one to the item grid.
	if (!IsValid(RelicClaimButton) && IsValid(WidgetTree) && IsValid(ItemList))
	{
		RelicClaimButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RelicClaimButton"));
		RelicClaimButton->SetBackgroundColor(FLinearColor(0.35f, 0.16f, 0.12f, 1.0f));
		RelicClaimText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RelicClaimText"));
		RelicClaimText->SetAutoWrapText(true);
		RelicClaimButton->AddChild(RelicClaimText);
		RelicClaimButton->SetVisibility(ESlateVisibility::Collapsed);
		bRelicClaimButtonGenerated = true;
	}
	if (IsValid(RelicClaimButton))
	{
		RelicClaimButton->OnClicked.AddUniqueDynamic(this, &UASLootWidget::HandleRelicClaimClicked);
	}

	if (HeaderCloseButton) HeaderCloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	// SetLootContext may render the item grid before NativeConstruct creates this fallback button.
	if (bRelicClaimButtonGenerated) RenderedSlotCount = INDEX_NONE;
	BindLootInventory();
	RefreshLootHeader();
	RefreshLoot();
}

void UASLootWidget::NativeDestruct()
{
	UnbindLootInventory();
	if (HeaderCloseButton) HeaderCloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);

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
		|| Drag->ViewModel.Get() != ViewModel
		|| Panel->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition())) return true;
	if (ViewModel) ViewModel->DropLootItem(Drag->ItemId);
	return true;
}

void UASLootWidget::RefreshLootHeader()
{
	if (!ViewModel) return;
	const FASInventoryPresentation& P = ViewModel->Presentation;
	if (LootTitleText) { if (!ASItemViewBinding::HasView(this)) LootTitleText->SetText(P.Title); LootTitleText->SetColorAndOpacity(FSlateColor(P.TitleColor)); }
	if (ItemCountText) ItemCountText->SetText(FText::AsNumber(P.ItemCount));
	if (EmptyText) EmptyText->SetVisibility(P.ItemCount == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UASLootWidget::RefreshLoot()
{
	RefreshLootHeader();
	if (!ItemList || !ViewModel)	
	{
		RefreshRelicRewardButton();
		return;
	}

	const int32 CurrentSlotCount = ViewModel->ContainerSlots.Num();
	const bool bRebuildItemList = RenderedSlotCount != CurrentSlotCount;
	if (bRebuildItemList)
	{
		ItemList->ClearChildren();
		RenderedSlotCount = CurrentSlotCount;
		const TSubclassOf<UASLootEntryWidget> EntryClass = EntryWidgetClass ? EntryWidgetClass.Get() : DefaultEntryWidgetClass.LoadSynchronous();
		if (EntryClass)
		{
			for (UASItemSlotViewModel* SlotModel : ViewModel->ContainerSlots)
			{
				UASLootEntryWidget* Entry = CreateWidget<UASLootEntryWidget>(GetOwningPlayer(), EntryClass);
				if (!Entry || !SlotModel) continue;
				Entry->SetViewModel(SlotModel);
				Entry->SetLootSource(this);
				if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(ItemList))
				{
					UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Entry,
						SlotModel->GetSlotIndex() / FMath::Max(1, Columns),
						SlotModel->GetSlotIndex() % FMath::Max(1, Columns));
					GridSlot->SetHorizontalAlignment(HAlign_Fill);
					GridSlot->SetVerticalAlignment(VAlign_Fill);
				}
				else if (UWrapBox* Tiles = Cast<UWrapBox>(ItemList))
				{
					USizeBox* Tile = WidgetTree->ConstructWidget<USizeBox>();
					Tile->SetWidthOverride(LootTileSize); Tile->SetHeightOverride(LootTileSize); Tile->SetContent(Entry);
					Tiles->AddChildToWrapBox(Tile);
				}
				else
				{
					ItemList->AddChild(Entry);
				}
			}
		}
	}

	RefreshRelicRewardButton();
	if (bRebuildItemList && bRelicClaimButtonGenerated && IsValid(RelicClaimButton)
		&& LootChest.IsValid() && LootChest->HasUnclaimedRelicReward())
	{
		if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(ItemList))
		{
			const int32 ButtonSlot = ViewModel->ContainerSlots.Num();
			UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(RelicClaimButton,
				ButtonSlot / FMath::Max(1, Columns), ButtonSlot % FMath::Max(1, Columns));
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
		else if (UWrapBox* Tiles = Cast<UWrapBox>(ItemList))
		{
			UWrapBoxSlot* RewardSlot = Tiles->AddChildToWrapBox(RelicClaimButton);
			RewardSlot->SetFillSpanWhenLessThan(TNumericLimits<float>::Max());
		}
		else
		{
			ItemList->AddChild(RelicClaimButton);
		}
	}
}

void UASLootWidget::BindLootInventory()
{
	if (!ViewModel) ViewModel = NewObject<UASInventoryViewModel>(this);
	if (ViewModel->GetInventory() != LootInventory)
	{
		ViewModel->Initialize(LootInventory, GetOwningPlayer());
		RenderedSlotCount = INDEX_NONE;
	}
	ASItemViewBinding::Set(this, TEXT("InventoryVM"), ViewModel);
	ViewModel->RemoveAllFieldValueChangedDelegates(this);
	using FFields = UASInventoryViewModel::FFieldNotificationClassDescriptor;
	for (UE::FieldNotification::FFieldId Field : {FFields::Presentation, FFields::ContainerSlots})
		ViewModel->AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this,
			[this](UObject*, UE::FieldNotification::FFieldId) { RefreshLoot(); }));
}

void UASLootWidget::UnbindLootInventory()
{
	if (ViewModel) { ViewModel->RemoveAllFieldValueChangedDelegates(this); ViewModel->Shutdown(); }
	RenderedSlotCount = INDEX_NONE;
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
