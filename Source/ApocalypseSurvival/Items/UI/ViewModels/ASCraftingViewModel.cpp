#include "Items/UI/ViewModels/ASCraftingViewModel.h"
#include "AbilitySystemComponent.h"
#include "Engine/Texture2D.h"
#include "Items/UI/ViewModels/ASCraftingEntryViewModel.h"
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
	BuildRecipeEntries();
	UE_MVVM_SET_PROPERTY_VALUE(bAwaitingResult, false);
	UE_MVVM_SET_PROPERTY_VALUE(Status, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(SelectedRecipe, RecipeEntries.IsEmpty() ? NAME_None : RecipeEntries[0]->GetRecipeId());
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
	for (UASCraftingEntryViewModel* Entry : RecipeEntries) Entry->Release();
	for (UASCraftingEntryViewModel* Entry : MaterialEntries) Entry->Release();
	RecipeEntries.Reset(); MaterialEntries.Reset(); RequiredQuantities.Reset(); DisplayedRecipe.Reset();
}

void UASCraftingViewModel::BeginDestroy() { Shutdown(); Super::BeginDestroy(); }

void UASCraftingViewModel::SelectRecipe(FName Id)
{
	if (bAwaitingResult || SelectedRecipe == Id) return;
	if (!Id.IsNone() && !RecipeEntries.ContainsByPredicate([Id](const UASCraftingEntryViewModel* Entry) { return Entry && Entry->GetRecipeId() == Id; })) return;
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

namespace
{
enum class ECraftingCategory : uint8 { Melee, Ranged, OtherWeapon, Bag, Material, Ammo, Consumable, Other };

ECraftingCategory CategoryOf(const UASItemDefinition* Item)
{
	if (!Item) return ECraftingCategory::Other;
	switch (Item->ItemType)
	{
	case EASItemType::Weapon:
		return Item->WeaponType == EASWeaponType::Melee ? ECraftingCategory::Melee
			: Item->WeaponType == EASWeaponType::Ranged ? ECraftingCategory::Ranged : ECraftingCategory::OtherWeapon;
	case EASItemType::Bag: return ECraftingCategory::Bag;
	case EASItemType::CraftingMaterial: return ECraftingCategory::Material;
	case EASItemType::Ammo: return ECraftingCategory::Ammo;
	case EASItemType::Consumable: return ECraftingCategory::Consumable;
	default: return ECraftingCategory::Other;
	}
}

FText CategoryFor(const UASItemDefinition* Item)
{
	switch (CategoryOf(Item))
	{
	case ECraftingCategory::Melee: return NSLOCTEXT("ASCrafting", "Melee", "근접 무기");
	case ECraftingCategory::Ranged: return NSLOCTEXT("ASCrafting", "Ranged", "원거리 무기");
	case ECraftingCategory::OtherWeapon: return NSLOCTEXT("ASCrafting", "Other", "기타 무기");
	case ECraftingCategory::Bag: return NSLOCTEXT("ASCrafting", "Bags", "가방");
	case ECraftingCategory::Material: return NSLOCTEXT("ASCrafting", "CraftingMaterials", "제작 재료");
	case ECraftingCategory::Ammo: return NSLOCTEXT("ASCrafting", "Ammo", "탄약");
	case ECraftingCategory::Consumable: return NSLOCTEXT("ASCrafting", "Consumables", "소모품");
	default: return NSLOCTEXT("ASCrafting", "OtherItems", "기타");
	}
}
}

void UASCraftingViewModel::BuildRecipeEntries()
{
	TArray<TObjectPtr<UASCraftingEntryViewModel>> Entries;
	const auto Available = Crafting.IsValid() ? Crafting->GetAvailableRecipes() : TArray<UASCraftingRecipeDefinition*>();
	for (ECraftingCategory Category : {ECraftingCategory::Melee, ECraftingCategory::Ranged, ECraftingCategory::OtherWeapon,
		ECraftingCategory::Bag, ECraftingCategory::Material, ECraftingCategory::Ammo, ECraftingCategory::Consumable, ECraftingCategory::Other})
	{
		bool bFirst = true;
		for (const UASCraftingRecipeDefinition* Recipe : Available)
		{
			UASItemDefinition* Item = Recipe->Result.ItemDefinition;
			if (CategoryOf(Item) != Category) continue;
			auto* Entry = NewObject<UASCraftingEntryViewModel>(this);
			FText Name = !Recipe->DisplayName.IsEmpty() ? Recipe->DisplayName : Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName : FText::FromName(Recipe->RecipeId);
			if (Recipe->Result.Quantity > 1) Name = FText::Format(NSLOCTEXT("ASCrafting", "ResultNameQuantity", "{0} × {1}"), Name, FText::AsNumber(Recipe->Result.Quantity));
			Entry->Initialize(this, Item, Recipe->RecipeId, Name, bFirst ? CategoryFor(Item) : FText::GetEmpty());
			Entries.Add(Entry); bFirst = false;
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(RecipeEntries, Entries);
}

void UASCraftingViewModel::BuildMaterialEntries(const UASCraftingRecipeDefinition* Recipe)
{
	for (UASCraftingEntryViewModel* Entry : MaterialEntries) Entry->Release();
	RequiredQuantities.Reset();
	TArray<TObjectPtr<UASCraftingEntryViewModel>> Entries;
	TMap<UASItemDefinition*, int32> Indices;
	if (Recipe) for (const FASItemGrant& Ingredient : Recipe->Ingredients)
	{
		UASItemDefinition* Item = Ingredient.ItemDefinition;
		if (!IsValid(Item)) continue;
		if (const int32* Existing = Indices.Find(Item)) RequiredQuantities[*Existing] += Ingredient.Quantity;
		else
		{
			Indices.Add(Item, Entries.Num());
			auto* Entry = NewObject<UASCraftingEntryViewModel>(this);
			Entry->Initialize(this, Item, NAME_None, Item->DisplayName);
			Entries.Add(Entry); RequiredQuantities.Add(Ingredient.Quantity);
		}
	}
	DisplayedRecipe = Recipe;
	UE_MVVM_SET_PROPERTY_VALUE(MaterialEntries, Entries);
}

void UASCraftingViewModel::HandleEntryIconChanged(UASCraftingEntryViewModel* Entry)
{
	if (Entry && !Entry->GetRecipeId().IsNone() && Entry->GetRecipeId() == SelectedRecipe)
		UE_MVVM_SET_PROPERTY_VALUE(ResultIcon, Entry->Presentation.Icon);
}

void UASCraftingViewModel::Refresh()
{
	const UASCraftingRecipeDefinition* Recipe = Crafting.IsValid() ? Crafting->FindRecipe(SelectedRecipe) : nullptr;
	const FASCraftingPreview Preview = Crafting.IsValid() ? Crafting->BuildPreview() : FASCraftingPreview();
	const EASCraftingResult Availability = Crafting.IsValid() ? Preview.GetResult(SelectedRecipe) : EASCraftingResult::InvalidPlayer;
	UE_MVVM_SET_PROPERTY_VALUE(bCanCraft, !bAwaitingResult && Availability == EASCraftingResult::Success);
	UE_MVVM_SET_PROPERTY_VALUE(AvailabilityText, Availability == EASCraftingResult::Success ? FText::GetEmpty() : UASCraftingComponent::GetResultText(Availability));
	if (DisplayedRecipe.Get() != Recipe || (!Recipe && !MaterialEntries.IsEmpty())) BuildMaterialEntries(Recipe);
	for (UASCraftingEntryViewModel* Entry : RecipeEntries)
	{
		const EASCraftingResult Result = Crafting.IsValid() ? Preview.GetResult(Entry->GetRecipeId()) : EASCraftingResult::InvalidPlayer;
		const FText Label = Result == EASCraftingResult::Success ? NSLOCTEXT("ASCrafting", "Available", "제작 가능")
			: Result == EASCraftingResult::MissingMaterials ? NSLOCTEXT("ASCrafting", "Missing", "재료 부족")
			: Result == EASCraftingResult::InventoryFull ? NSLOCTEXT("ASCrafting", "NoSpace", "공간 부족") : NSLOCTEXT("ASCrafting", "Unavailable", "제작 불가");
		Entry->Update(Label, Result == EASCraftingResult::Success, !bAwaitingResult, Entry->GetRecipeId() == SelectedRecipe);
	}
	for (int32 Index = 0; Index < MaterialEntries.Num(); ++Index)
	{
		const int64 Owned = Preview.GetItemCount(MaterialEntries[Index]->GetDefinition());
		MaterialEntries[Index]->Update(FText::Format(NSLOCTEXT("ASCrafting", "MaterialQuantity", "{0} / {1}"), FText::AsNumber(Owned), FText::AsNumber(RequiredQuantities[Index])), Owned >= RequiredQuantities[Index], true);
	}
	UASItemDefinition* Item = Recipe ? Recipe->Result.ItemDefinition.Get() : nullptr;
	const FText Name = Recipe ? Item && !Item->DisplayName.IsEmpty() ? Item->DisplayName : !Recipe->DisplayName.IsEmpty() ? Recipe->DisplayName : FText::FromName(SelectedRecipe) : NSLOCTEXT("ASCrafting", "NoItemSelection", "제작할 아이템을 선택하세요");
	UE_MVVM_SET_PROPERTY_VALUE(SelectedName, Name);
	UE_MVVM_SET_PROPERTY_VALUE(SelectedCategory, CategoryFor(Item));
	UE_MVVM_SET_PROPERTY_VALUE(Details, Item ? Item->Description : NSLOCTEXT("ASCrafting", "Empty", "등록된 제작 레시피가 없습니다."));
	FText Stats;
	if (Item && Item->ItemType == EASItemType::Weapon)
		Stats = FText::Format(NSLOCTEXT("ASCrafting", "WeaponStats", "기본 공격력   {0}\n공격 간격   {1}초\n사거리   {2}m"), FText::AsNumber(Item->BaseDamage), FText::AsNumber(Item->AttackInterval), FText::AsNumber(Item->AttackRange / 100.0f));
	else if (Item && Item->ItemType == EASItemType::Bag)
		Stats = FText::Format(NSLOCTEXT("ASCrafting", "BagStats", "추가 보관 공간   +{0}칸"), FText::AsNumber(Item->BagAdditionalSlots));
	else if (Item)
		Stats = FText::Format(NSLOCTEXT("ASCrafting", "StackStats", "최대 중첩   {0}개"), FText::AsNumber(Item->MaxStackSize));
	UE_MVVM_SET_PROPERTY_VALUE(WeaponStats, Stats);
	UE_MVVM_SET_PROPERTY_VALUE(OwnedResultText, Recipe ? FText::Format(NSLOCTEXT("ASCrafting", "OutputAndOwned", "제작 수량: {0}개\n보유 수량: {1}개"), FText::AsNumber(Recipe->Result.Quantity), FText::AsNumber(Preview.GetItemCount(Item))) : FText::GetEmpty());
	UTexture2D* Icon = nullptr;
	for (UASCraftingEntryViewModel* Entry : RecipeEntries) if (Entry->GetRecipeId() == SelectedRecipe) { Icon = Entry->Presentation.Icon; break; }
	UE_MVVM_SET_PROPERTY_VALUE(ResultIcon, TObjectPtr<UTexture2D>(Icon));
}
