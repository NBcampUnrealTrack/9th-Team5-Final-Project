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
#include "Items/UI/ASCraftingWidget.h"

namespace
{
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
	if (CraftingWidget) CraftingWidget->AddToViewport(15);
}

void UASCraftingComponent::CloseCrafting()
{
	if (CraftingWidget) CraftingWidget->RemoveFromParent();
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
	case EASCraftingResult::Success: return NSLOCTEXT("ASCrafting", "Success", "제작 완료! 무기가 가방에 추가되었습니다.");
	case EASCraftingResult::MissingMaterials: return NSLOCTEXT("ASCrafting", "Materials", "재료가 부족합니다.");
	case EASCraftingResult::InventoryFull: return NSLOCTEXT("ASCrafting", "Full", "무기를 보관할 가방 공간이 부족합니다.");
	case EASCraftingResult::InvalidPlayer: return NSLOCTEXT("ASCrafting", "Player", "현재 제작할 수 없는 상태입니다.");
	case EASCraftingResult::UnknownRecipe: return NSLOCTEXT("ASCrafting", "Unknown", "등록된 레시피를 선택해 주세요.");
	default: return NSLOCTEXT("ASCrafting", "Invalid", "레시피 설정을 확인해 주세요.");
	}
}
