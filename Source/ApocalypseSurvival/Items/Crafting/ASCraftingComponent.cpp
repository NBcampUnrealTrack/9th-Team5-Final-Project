#include "Items/Crafting/ASCraftingComponent.h"

#include "AbilitySystemComponent.h"
#include "Core/Framework/ASGameState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ASAbilityTags.h"
#include "Items/Crafting/ASCraftingRecipeDefinition.h"
#include "Items/Crafting/ASCraftingCatalog.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/UI/ASCraftingWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Core/Framework/ASPlayerController.h"

namespace
{
	struct FMaterialStack
	{
		int64 EndQuantity;
		int32 Slot;
		int32 Quantity;
	};
	struct FMaterialContents
	{
		int64 Quantity = 0;
		TArray<FMaterialStack> Stacks;
	};
	struct FInventoryPreview
	{
		TMap<const UASItemDefinition*, FMaterialContents> Materials;
		TMap<int32, int32> OccupantsPerSlot;
		TMap<const UASItemDefinition*, int64> StackSpace;
		int32 SlotLimit = 0;
		int32 FreeSlotCount = 0;

		EASCraftingResult Evaluate(const UASCraftingRecipeDefinition& Recipe) const
		{
			TMap<const UASItemDefinition*, int64> Required;
			for (const FASItemGrant& Cost : Recipe.Ingredients) Required.FindOrAdd(Cost.ItemDefinition) += Cost.Quantity;
			for (const auto& Cost : Required)
			{
				const FMaterialContents* Contents = Materials.Find(Cost.Key);
				if (!Contents || Contents->Quantity < Cost.Value) return EASCraftingResult::MissingMaterials;
			}
			if (SlotLimit <= 0) return EASCraftingResult::Success;

			const UASItemDefinition* ResultItem = Recipe.Result.ItemDefinition;
			int64 ResultSpace = StackSpace.FindRef(ResultItem);
			int32 FreedSlots = 0;
			// Match the exchange planner's input order. Consuming inputs can free slots and
			// change merge capacity when the result is also one of the ingredients.
			TMap<int32, int32> RemovedPerSlot;
			for (const auto& Cost : Required)
			{
				for (const FMaterialStack& Stack : Materials.FindChecked(Cost.Key).Stacks)
				{
					const int64 Consumed = FMath::Clamp<int64>(Cost.Value - (Stack.EndQuantity - Stack.Quantity), 0, Stack.Quantity);
					if (Consumed == 0) break;
					const int32 Remaining = Stack.Quantity - static_cast<int32>(Consumed);
					if (Cost.Key == ResultItem)
					{
						ResultSpace -= FMath::Max(0, ResultItem->MaxStackSize - Stack.Quantity);
						if (Remaining > 0) ResultSpace += FMath::Max(0, ResultItem->MaxStackSize - Remaining);
					}
					if (Remaining == 0 && Stack.Slot < SlotLimit
						&& ++RemovedPerSlot.FindOrAdd(Stack.Slot) == OccupantsPerSlot.FindChecked(Stack.Slot)) ++FreedSlots;
				}
			}
			const int64 Unstacked = FMath::Max<int64>(0, static_cast<int64>(Recipe.Result.Quantity) - ResultSpace);
			const int64 RequiredSlots = (Unstacked + ResultItem->MaxStackSize - 1) / ResultItem->MaxStackSize;
			return RequiredSlots <= FreeSlotCount + FreedSlots ? EASCraftingResult::Success : EASCraftingResult::InventoryFull;
		}
	};

	EASCraftingResult ToCraftingResult(EASInventoryExchangeResult Result)
	{
		switch (Result)
		{
		case EASInventoryExchangeResult::Success: return EASCraftingResult::Success;
		case EASInventoryExchangeResult::MissingMaterials: return EASCraftingResult::MissingMaterials;
		case EASInventoryExchangeResult::InventoryFull: return EASCraftingResult::InventoryFull;
		case EASInventoryExchangeResult::NoAuthority: return EASCraftingResult::InvalidPlayer;
		default: return EASCraftingResult::InvalidRecipe;
		}
	}
}

UASCraftingComponent::UASCraftingComponent()
{
	DefaultCraftingWidgetClass = FSoftObjectPath(TEXT("/Game/ApocalypseSurvival/Items/UI/WBP_Crafting.WBP_Crafting_C"));
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UASInventoryComponent* UASCraftingComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UASInventoryComponent>() : nullptr;
}

UASCraftingRecipeDefinition* UASCraftingComponent::FindRecipe(FName RecipeId) const
{
	return IsValid(Catalog) ? Catalog->FindRecipe(RecipeId) : nullptr;
}

TArray<UASCraftingRecipeDefinition*> UASCraftingComponent::GetAvailableRecipes() const
{
	return IsValid(Catalog) ? Catalog->GetAvailableRecipes() : TArray<UASCraftingRecipeDefinition*>();
}

bool UASCraftingComponent::CanPlayerCraft() const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	UASInventoryComponent* Inventory = GetInventory();
	if (!Controller || !Controller->GetPawn() || !Inventory || !GetWorld() || GetWorld()->bIsTearingDown) return false;
	if (const AASGameState* State = GetWorld()->GetGameState<AASGameState>(); State && State->IsGameEnded()) return false;
	const UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(Inventory, Controller->GetPawn());
	return ASC && !ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead);
}

EASCraftingResult UASCraftingComponent::CanCraft(FName RecipeId) const
{
	const UASCraftingRecipeDefinition* Recipe = FindRecipe(RecipeId);
	if (!Recipe) return EASCraftingResult::UnknownRecipe;
	FString Error;
	if (!Recipe->ValidateRecipe(Error)) return EASCraftingResult::InvalidRecipe;
	if (!CanPlayerCraft()) return EASCraftingResult::InvalidPlayer;
	return ToCraftingResult(GetInventory()->CanExchangeItems(Recipe->Ingredients, {Recipe->Result}));
}

FASCraftingPreview UASCraftingComponent::BuildPreview() const
{
	FASCraftingPreview Preview;
	FInventoryPreview Contents;
	if (const UASInventoryComponent* Inventory = GetInventory())
	{
		Contents.SlotLimit = Inventory->GetMaxSlots();
		for (const UASItemInstance* Item : Inventory->GetItemInstances())
		{
			if (!IsValid(Item) || !Item->IsValidItemInstance()) continue;
			const UASItemDefinition* Definition = Item->GetItemDefinition();
			Preview.OwnedCounts.FindOrAdd(Definition) += Item->GetQuantity();
			if (Item->GetInventorySlot() < 0) continue;
			++Contents.OccupantsPerSlot.FindOrAdd(Item->GetInventorySlot());
			Contents.StackSpace.FindOrAdd(Definition) += FMath::Max(0, Definition->MaxStackSize - Item->GetQuantity());
			if (Definition->ItemType == EASItemType::CraftingMaterial)
			{
				FMaterialContents& Material = Contents.Materials.FindOrAdd(Definition);
				Material.Quantity += Item->GetQuantity();
				Material.Stacks.Add({Material.Quantity, Item->GetInventorySlot(), Item->GetQuantity()});
			}
		}
		int32 OccupiedSlots = 0;
		for (const auto& Slot : Contents.OccupantsPerSlot) if (Slot.Key < Contents.SlotLimit) ++OccupiedSlots;
		Contents.FreeSlotCount = FMath::Max(0, Contents.SlotLimit - OccupiedSlots);
	}
	const bool bPlayerCanCraft = CanPlayerCraft();
	for (const UASCraftingRecipeDefinition* Recipe : GetAvailableRecipes())
	{
		FString Error;
		const EASCraftingResult Result = !Recipe->ValidateRecipe(Error) ? EASCraftingResult::InvalidRecipe
			: !bPlayerCanCraft ? EASCraftingResult::InvalidPlayer : Contents.Evaluate(*Recipe);
		Preview.Results.Add(Recipe->RecipeId, Result);
	}
	return Preview;
}

void UASCraftingComponent::RequestCraft(FName RecipeId)
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (Controller && Controller->IsLocalController()) ServerCraft(RecipeId);
}

void UASCraftingComponent::ServerCraft_Implementation(FName RecipeId)
{
	EASCraftingResult Result = CanCraft(RecipeId);
	if (Result == EASCraftingResult::Success)
	{
		const UASCraftingRecipeDefinition* Recipe = FindRecipe(RecipeId);
		Result = ToCraftingResult(GetInventory()->TryExchangeItems(Recipe->Ingredients, {Recipe->Result}));
	}
	ClientCraftFinished(RecipeId, Result);
}

void UASCraftingComponent::ClientCraftFinished_Implementation(FName RecipeId, EASCraftingResult Result)
{
	OnCraftingFinished.Broadcast(RecipeId, Result);
}

void UASCraftingComponent::ToggleCrafting()
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->IsLocalController()) return;
	if (CraftingWidget && CraftingWidget->IsInViewport()) { CloseCrafting(); return; }
	if (!CraftingWidget) CraftingWidget = CreateWidget<UASCraftingWidget>(Controller, CraftingWidgetClass ? CraftingWidgetClass.Get() : DefaultCraftingWidgetClass.LoadSynchronous());
	if (CraftingWidget)
	{
		CraftingWidget->AddToViewport(15);
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(CraftingWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		Mode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(Mode);
	}
}

void UASCraftingComponent::CloseCrafting()
{
	if (!IsCraftingOpen()) return;
	CraftingWidget->RemoveFromParent();
	AASPlayerController* Controller = Cast<AASPlayerController>(GetOwner());
	if (!Controller || !GetWorld() || GetWorld()->bIsTearingDown || Controller->IsInGameMenuOpen()) return;
	if (const AASGameState* State = GetWorld()->GetGameState<AASGameState>(); State && State->IsGameEnded()) return;
	UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

bool UASCraftingComponent::IsCraftingOpen() const
{
	return CraftingWidget && CraftingWidget->IsInViewport();
}

void UASCraftingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseCrafting();
	Super::EndPlay(EndPlayReason);
}

FText UASCraftingComponent::GetResultText(EASCraftingResult Result)
{
	switch (Result)
	{
	case EASCraftingResult::Success: return NSLOCTEXT("ASCrafting", "ItemsCreated", "제작 완료! 아이템이 인벤토리에 추가되었습니다.");
	case EASCraftingResult::MissingMaterials: return NSLOCTEXT("ASCrafting", "Materials", "재료가 부족합니다.");
	case EASCraftingResult::InventoryFull: return NSLOCTEXT("ASCrafting", "ResultInventoryFull", "제작 결과를 보관할 인벤토리 공간이 부족합니다.");
	case EASCraftingResult::InvalidPlayer: return NSLOCTEXT("ASCrafting", "Player", "현재 제작할 수 없는 상태입니다.");
	case EASCraftingResult::UnknownRecipe: return NSLOCTEXT("ASCrafting", "Unknown", "등록된 레시피를 선택해 주세요.");
	default: return NSLOCTEXT("ASCrafting", "Invalid", "레시피 설정을 확인해 주세요.");
	}
}
