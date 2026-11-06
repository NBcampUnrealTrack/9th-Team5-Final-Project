#include "Items/UI/ViewModels/ASPlayerMenuViewModel.h"
#include "Core/Framework/ASPlayerController.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Crafting/ASCraftingComponent.h"

void UASPlayerMenuViewModel::Initialize(APlayerController* Controller, EASPlayerMenuTab Screen)
{
	Player = Controller;
	ScreenTab = Screen == EASPlayerMenuTab::Crafting ? EASPlayerMenuTab::Crafting : EASPlayerMenuTab::Inventory;
	SetActiveTab(Screen);
}

void UASPlayerMenuViewModel::SetActiveTab(EASPlayerMenuTab Tab)
{
	if (static_cast<uint8>(Tab) > static_cast<uint8>(EASPlayerMenuTab::Crafting)) return;
	FASPlayerMenuPresentation Value;
	Value.ActiveTab = Tab;
	Value.ContentIndex = Tab == EASPlayerMenuTab::Crafting ? INDEX_NONE : static_cast<int32>(Tab);
	const FText Labels[] = {NSLOCTEXT("PlayerMenu", "InventoryTab", "인벤토리"), NSLOCTEXT("PlayerMenu", "QuestTab", "퀘스트"), NSLOCTEXT("PlayerMenu", "ModuleTab", "모듈"), NSLOCTEXT("PlayerMenu", "CraftingTab", "제작")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FASPlayerMenuTabPresentation& Entry = Value.Tabs.AddDefaulted_GetRef();
		Entry.Tab = static_cast<EASPlayerMenuTab>(Index); Entry.Label = Labels[Index]; Entry.bActive = Entry.Tab == Tab;
	}
	UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
}

void UASPlayerMenuViewModel::RequestTab(EASPlayerMenuTab Tab)
{
	if (static_cast<uint8>(Tab) > static_cast<uint8>(EASPlayerMenuTab::Crafting)) return;
	if (ScreenTab != EASPlayerMenuTab::Crafting && Tab != EASPlayerMenuTab::Crafting) { SetActiveTab(Tab); return; }
	if (ScreenTab == EASPlayerMenuTab::Crafting && Tab == EASPlayerMenuTab::Crafting) return;
	AASPlayerController* Controller = Cast<AASPlayerController>(Player.Get());
	if (!Controller) return;
	if (Tab == EASPlayerMenuTab::Crafting)
	{
		if (Controller->GetCraftingComponent() && !Controller->GetCraftingComponent()->IsCraftingOpen()) Controller->ToggleCrafting();
	}
	else if (UASItemInteractionComponent* Interaction = Controller->GetItemInteractionComponent()) Interaction->OpenInventoryMenuTab(Tab);
}

void UASPlayerMenuViewModel::RequestClose()
{
	AASPlayerController* Controller = Cast<AASPlayerController>(Player.Get());
	if (!Controller) return;
	if (ScreenTab == EASPlayerMenuTab::Crafting)
	{
		if (UASCraftingComponent* Crafting = Controller->GetCraftingComponent()) Crafting->CloseCrafting();
	}
	else if (UASItemInteractionComponent* Interaction = Controller->GetItemInteractionComponent()) Interaction->CloseInventory();
}
