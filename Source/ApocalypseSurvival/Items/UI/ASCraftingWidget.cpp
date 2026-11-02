#include "Items/UI/ASCraftingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "Core/Framework/ASPlayerController.h"
#include "Core/Framework/ASGameState.h"
#include "Engine/World.h"
#include "GAS/ASAbilityTags.h"
#include "Items/Crafting/ASCraftingRecipeDefinition.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/GAS/ASItemAbilityLibrary.h"

TSharedRef<SWidget> UASCraftingWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.98f));
		Panel->SetPadding(FMargin(20));
		UCanvasPanelSlot* Position = Canvas->AddChildToCanvas(Panel);
		Position->SetAnchors(FAnchors(0.5f, 0.5f));
		Position->SetAlignment(FVector2D(0.5f, 0.5f));
		Position->SetAutoSize(true);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		Panel->SetContent(Content);
		auto AddText = [&](const FText& Text)
		{
			UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
			Block->SetText(Text);
			Block->SetWrapTextAt(420);
			FSlateFontInfo Font = Block->GetFont();
			Font.Size = 18;
			Block->SetFont(Font);
			Content->AddChildToVerticalBox(Block)->SetPadding(FMargin(0, 6));
			return Block;
		};
		AddText(NSLOCTEXT("ASCrafting", "Title", "무기 제작"));
		RecipePicker = WidgetTree->ConstructWidget<UComboBoxString>();
		Content->AddChildToVerticalBox(RecipePicker)->SetPadding(FMargin(0, 8));
		RecipePicker->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleSelection);
		Details = AddText(FText::GetEmpty());
		Status = AddText(FText::GetEmpty());
		UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
		Content->AddChildToVerticalBox(Actions);
		CraftButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* CraftLabel = WidgetTree->ConstructWidget<UTextBlock>();
		CraftLabel->SetText(NSLOCTEXT("ASCrafting", "Craft", "제작"));
		CraftButton->SetContent(CraftLabel);
		CraftButton->OnClicked.AddDynamic(this, &ThisClass::Craft);
		Actions->AddChildToHorizontalBox(CraftButton);
		UButton* CloseButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>();
		CloseLabel->SetText(NSLOCTEXT("ASCrafting", "Close", "닫기"));
		CloseButton->SetContent(CloseLabel);
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::Close);
		Actions->AddChildToHorizontalBox(CloseButton);
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}

void UASCraftingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Crafting = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<UASCraftingComponent>() : nullptr;
	Inventory = Crafting ? Crafting->GetInventory() : nullptr;
	bAwaitingResult = false;
	if (Crafting) Crafting->OnCraftingFinished.AddUniqueDynamic(this, &ThisClass::HandleFinished);
	if (Inventory) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	ObservedController = Cast<AASPlayerController>(GetOwningPlayer());
	if (ObservedController.IsValid()) ObservedController->OnLocalPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	BindPlayerStateSources();
	RecipePicker->ClearOptions();
	if (Crafting)
	{
		for (const UASCraftingRecipeDefinition* Recipe : Crafting->GetAvailableRecipes())
			RecipePicker->AddOption(Recipe->RecipeId.ToString());
	}
	if (RecipePicker->GetOptionCount() > 0) RecipePicker->SetSelectedIndex(0);
	Status->SetText(FText::GetEmpty());
	Refresh();
}

void UASCraftingWidget::NativeDestruct()
{
	if (Crafting) Crafting->OnCraftingFinished.RemoveDynamic(this, &ThisClass::HandleFinished);
	if (Inventory) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::Refresh);
	if (ObservedController.IsValid()) ObservedController->OnLocalPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	ObservedController.Reset();
	UnbindPlayerStateSources();
	Super::NativeDestruct();
}

void UASCraftingWidget::BindPlayerStateSources()
{
	UnbindPlayerStateSources();
	// The controller also broadcasts when the pawn's ASC becomes ready after replication.
	APawn* Pawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
	ObservedASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(Inventory, Pawn);
	if (ObservedASC.IsValid())
	{
		DeadTagChangedHandle = ObservedASC->RegisterGameplayTagEvent(ASGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleDeadTagChanged);
	}
	ObservedGameState = GetWorld() ? GetWorld()->GetGameState<AASGameState>() : nullptr;
	if (ObservedGameState.IsValid()) ObservedGameState->OnPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandlePhaseChanged);
}

void UASCraftingWidget::UnbindPlayerStateSources()
{
	if (ObservedASC.IsValid() && DeadTagChangedHandle.IsValid())
		ObservedASC->RegisterGameplayTagEvent(ASGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).Remove(DeadTagChangedHandle);
	DeadTagChangedHandle.Reset();
	ObservedASC.Reset();
	if (ObservedGameState.IsValid()) ObservedGameState->OnPhaseChanged.RemoveDynamic(this, &ThisClass::HandlePhaseChanged);
	ObservedGameState.Reset();
}

void UASCraftingWidget::HandlePawnChanged(APawn* NewPawn)
{
	BindPlayerStateSources();
	Refresh();
}

void UASCraftingWidget::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	Refresh();
}

void UASCraftingWidget::HandlePhaseChanged(EGamePhase NewPhase)
{
	Refresh();
}

void UASCraftingWidget::HandleSelection(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (Status) Status->SetText(FText::GetEmpty());
	Refresh();
}

void UASCraftingWidget::Refresh()
{
	if (!RecipePicker || !Details || !CraftButton) return;
	const FName Id(*RecipePicker->GetSelectedOption());
	const UASCraftingRecipeDefinition* Recipe = Crafting ? Crafting->FindRecipe(Id) : nullptr;
	const EASCraftingResult Availability = Crafting ? Crafting->CanCraft(Id) : EASCraftingResult::InvalidPlayer;
	CraftButton->SetIsEnabled(!bAwaitingResult && Availability == EASCraftingResult::Success);
	if (!Recipe)
	{
		Details->SetText(NSLOCTEXT("ASCrafting", "Empty", "등록된 제작 레시피가 없습니다."));
		return;
	}
	FString Text = Recipe->DisplayName.IsEmpty() ? Id.ToString() : Recipe->DisplayName.ToString();
	if (IsValid(Recipe->Result.ItemDefinition))
		Text += FString::Printf(TEXT("\n결과: %s × %d\n"), *Recipe->Result.ItemDefinition->DisplayName.ToString(), Recipe->Result.Quantity);
	TMap<UASItemDefinition*, int64> Totals;
	for (const FASItemGrant& Ingredient : Recipe->Ingredients)
		if (IsValid(Ingredient.ItemDefinition)) Totals.FindOrAdd(Ingredient.ItemDefinition) += Ingredient.Quantity;
	for (const auto& Ingredient : Totals)
	{
		Text += FString::Printf(TEXT("\n%s: %d / %lld"), *Ingredient.Key->DisplayName.ToString(),
			Inventory ? Inventory->GetItemCount(Ingredient.Key) : 0, Ingredient.Value);
	}
	if (Availability != EASCraftingResult::Success) Text += TEXT("\n\n") + UASCraftingComponent::GetResultText(Availability).ToString();
	Details->SetText(FText::FromString(Text));
}

void UASCraftingWidget::Craft()
{
	if (!Crafting || bAwaitingResult) return;
	const FName Id(*RecipePicker->GetSelectedOption());
	if (Crafting->CanCraft(Id) != EASCraftingResult::Success) { Refresh(); return; }
	bAwaitingResult = true;
	Status->SetText(NSLOCTEXT("ASCrafting", "Pending", "제작 중..."));
	Refresh();
	Crafting->RequestCraft(Id);
}

void UASCraftingWidget::HandleFinished(FName RecipeId, EASCraftingResult Result)
{
	bAwaitingResult = false;
	Status->SetText(UASCraftingComponent::GetResultText(Result));
	Refresh();
}

void UASCraftingWidget::Close()
{
	if (Crafting) Crafting->CloseCrafting();
}
