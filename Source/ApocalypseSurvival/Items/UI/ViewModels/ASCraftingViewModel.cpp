#include "Items/UI/ViewModels/ASCraftingViewModel.h"
#include "AbilitySystemComponent.h"
#include "Core/Framework/ASPlayerController.h"
#include "Core/Framework/ASGameState.h"
#include "Engine/World.h"
#include "GAS/ASAbilityTags.h"
#include "Items/Crafting/ASCraftingRecipeDefinition.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/GAS/ASItemAbilityLibrary.h"

void UASCraftingViewModel::Initialize(APlayerController* Controller)
{
	Shutdown();
	Player = Controller;
	Crafting = Controller ? Controller->FindComponentByClass<UASCraftingComponent>() : nullptr;
	Inventory = Crafting.IsValid() ? Crafting->GetInventory() : nullptr;
	if (Crafting.IsValid()) Crafting->OnCraftingFinished.AddUniqueDynamic(this, &ThisClass::HandleFinished);
	if (Inventory.IsValid()) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	ObservedController = Cast<AASPlayerController>(Controller);
	if (ObservedController.IsValid()) ObservedController->OnLocalPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	BindPlayerStateSources();
	TArray<FString> Options;
	if (Crafting.IsValid()) for (const UASCraftingRecipeDefinition* Recipe : Crafting->GetAvailableRecipes()) Options.Add(Recipe->RecipeId.ToString());
	UE_MVVM_SET_PROPERTY_VALUE(Recipes, Options);
	UE_MVVM_SET_PROPERTY_VALUE(bAwaitingResult, false);
	UE_MVVM_SET_PROPERTY_VALUE(Status, FText::GetEmpty());
	SelectRecipe(Options.IsEmpty() ? NAME_None : FName(*Options[0]));
	Refresh();
}

void UASCraftingViewModel::Shutdown()
{
	if (Crafting.IsValid()) Crafting->OnCraftingFinished.RemoveDynamic(this, &ThisClass::HandleFinished);
	if (Inventory.IsValid()) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::Refresh);
	if (ObservedController.IsValid()) ObservedController->OnLocalPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	UnbindPlayerStateSources();
	ObservedController.Reset(); Player.Reset(); Crafting.Reset(); Inventory.Reset();
	PendingRecipe = NAME_None;
}

void UASCraftingViewModel::BeginDestroy() { Shutdown(); Super::BeginDestroy(); }

void UASCraftingViewModel::SelectRecipe(FName Id)
{
	if (bAwaitingResult || SelectedRecipe == Id) return;
	if (!Id.IsNone() && !Recipes.Contains(Id.ToString())) return;
	UE_MVVM_SET_PROPERTY_VALUE(SelectedRecipe, Id);
	UE_MVVM_SET_PROPERTY_VALUE(Status, FText::GetEmpty());
	Refresh();
}

void UASCraftingViewModel::Craft()
{
	if (!Crafting.IsValid() || bAwaitingResult) return;
	if (Crafting->CanCraft(SelectedRecipe) != EASCraftingResult::Success) { Refresh(); return; }
	PendingRecipe = SelectedRecipe;
	UE_MVVM_SET_PROPERTY_VALUE(bAwaitingResult, true);
	UE_MVVM_SET_PROPERTY_VALUE(Status, NSLOCTEXT("ASCrafting", "Pending", "제작 중..."));
	Refresh();
	Crafting->RequestCraft(PendingRecipe);
}

void UASCraftingViewModel::HandleFinished(FName RecipeId, EASCraftingResult Result)
{
	if (!bAwaitingResult || RecipeId != PendingRecipe) return;
	PendingRecipe = NAME_None;
	UE_MVVM_SET_PROPERTY_VALUE(bAwaitingResult, false);
	UE_MVVM_SET_PROPERTY_VALUE(Status, UASCraftingComponent::GetResultText(Result));
	Refresh();
}

void UASCraftingViewModel::Close() { if (Crafting.IsValid()) Crafting->CloseCrafting(); }

void UASCraftingViewModel::BindPlayerStateSources()
{
	UnbindPlayerStateSources();
	// The controller also broadcasts when the pawn's ASC becomes ready after replication.
	APawn* Pawn = Player.Get() ? Player.Get()->GetPawn() : nullptr;
	ObservedASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(Inventory.Get(), Pawn);
	if (ObservedASC.IsValid())
	{
		DeadTagChangedHandle = ObservedASC->RegisterGameplayTagEvent(ASGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleDeadTagChanged);
	}
	ObservedGameState = Player.IsValid() && Player->GetWorld() ? Player->GetWorld()->GetGameState<AASGameState>() : nullptr;
	if (ObservedGameState.IsValid()) ObservedGameState->OnPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandlePhaseChanged);
}

void UASCraftingViewModel::UnbindPlayerStateSources()
{
	if (ObservedASC.IsValid() && DeadTagChangedHandle.IsValid())
		ObservedASC->RegisterGameplayTagEvent(ASGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).Remove(DeadTagChangedHandle);
	DeadTagChangedHandle.Reset();
	ObservedASC.Reset();
	if (ObservedGameState.IsValid()) ObservedGameState->OnPhaseChanged.RemoveDynamic(this, &ThisClass::HandlePhaseChanged);
	ObservedGameState.Reset();
}

void UASCraftingViewModel::HandlePawnChanged(APawn* NewPawn)
{
	BindPlayerStateSources();
	Refresh();
}

void UASCraftingViewModel::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	Refresh();
}

void UASCraftingViewModel::HandlePhaseChanged(EGamePhase NewPhase)
{
	Refresh();
}

void UASCraftingViewModel::Refresh()
{
	const FName Id = SelectedRecipe;
	const UASCraftingRecipeDefinition* Recipe = Crafting.IsValid() ? Crafting->FindRecipe(Id) : nullptr;
	const EASCraftingResult Availability = Crafting.IsValid() ? Crafting->CanCraft(Id) : EASCraftingResult::InvalidPlayer;
	UE_MVVM_SET_PROPERTY_VALUE(bCanCraft, !bAwaitingResult && Availability == EASCraftingResult::Success);
	if (!Recipe)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Details, NSLOCTEXT("ASCrafting", "Empty", "등록된 제작 레시피가 없습니다."));
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
			Inventory.IsValid() ? Inventory->GetItemCount(Ingredient.Key) : 0, Ingredient.Value);
	}
	if (Availability != EASCraftingResult::Success) Text += TEXT("\n\n") + UASCraftingComponent::GetResultText(Availability).ToString();
	const FText NewDetails = FText::FromString(Text);
	if (!Details.EqualTo(NewDetails)) { Details = NewDetails; UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Details); }
}

