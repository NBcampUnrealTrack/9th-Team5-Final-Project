#include "Items/UI/ViewModels/ASInventoryViewModel.h"
#include "Quests/ASQuestComponent.h"
#include "GameFramework/PlayerState.h"

#include "Items/UI/ViewModels/ASItemSlotViewModel.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Loot/ASLootChest.h"
#include "Core/Framework/ASPlayerController.h"
#include "Core/Settings/ASLocalSettingsSubsystem.h"
#include "Engine/LocalPlayer.h"

bool FASInventoryPresentation::operator==(const FASInventoryPresentation& O) const
{
	return StorageSlotCount == O.StorageSlotCount && ItemCount == O.ItemCount && Capacity.EqualTo(O.Capacity)
		&& Feedback.EqualTo(O.Feedback) && Title.EqualTo(O.Title) && TitleColor == O.TitleColor;
}

void UASInventoryViewModel::Initialize(UASInventoryComponent* Source, APlayerController* Controller)
{
	Shutdown();
	Inventory = Source;
	Player = Controller;
	Quests = Controller && Controller->PlayerState ? Controller->PlayerState->FindComponentByClass<UASQuestComponent>() : nullptr;
	if (Quests.IsValid()) Quests->OnQuestsChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	if (ULocalPlayer* Local = Controller ? Controller->GetLocalPlayer() : nullptr)
		if (UASLocalSettingsSubsystem* Settings = Local->GetSubsystem<UASLocalSettingsSubsystem>())
			Settings->OnKeyMappingsChanged.AddUObject(this, &ThisClass::Refresh);
	Interaction = Controller ? Controller->FindComponentByClass<UASItemInteractionComponent>() : nullptr;
	if (Source) Source->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	if (Interaction.IsValid() && IsPlayerSource()) Interaction->OnInventoryActionFailed.AddUniqueDynamic(this, &ThisClass::HandleActionFailed);
	Refresh();
}

void UASInventoryViewModel::Shutdown()
{
	if (Quests.IsValid()) Quests->OnQuestsChanged.RemoveDynamic(this, &ThisClass::Refresh);
	Quests.Reset();
	if (ULocalPlayer* Local = Player.IsValid() ? Player->GetLocalPlayer() : nullptr)
		if (UASLocalSettingsSubsystem* Settings = Local->GetSubsystem<UASLocalSettingsSubsystem>())
			Settings->OnKeyMappingsChanged.RemoveAll(this);
	if (Inventory.IsValid()) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::Refresh);
	if (Interaction.IsValid()) Interaction->OnInventoryActionFailed.RemoveDynamic(this, &ThisClass::HandleActionFailed);
	Inventory.Reset(); Player.Reset(); Interaction.Reset();
	for (const auto& Slot : SlotModels) Slot.Value->Release();
	SlotModels.Reset();
	QuickSlots.Reset(); StorageSlots.Reset(); ContainerSlots.Reset();
}

void UASInventoryViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

bool UASInventoryViewModel::IsPlayerSource() const
{
	return Inventory.IsValid() && Player.IsValid() && Inventory->GetOwner() == Player.Get();
}

FText UASInventoryViewModel::GetQuickSlotKeyText(int32 Index) const
{
	const AASPlayerController* PC = IsPlayerSource() ? Cast<AASPlayerController>(Player.Get()) : nullptr;
	const FKey Key = PC ? PC->GetQuickSlotKey(Index) : FKey();
	return Key.IsValid() ? Key.GetDisplayName() : FText::GetEmpty();
}

bool UASInventoryViewModel::IsExposedSlot(int32 Index) const
{
	return Inventory.IsValid() && (Inventory->IsValidItemSlot(Index)
		|| (IsPlayerSource() && (Index == UASInventoryComponent::WeaponSlotIndex || Index == UASInventoryComponent::BagSlotIndex)));
}

AASLootChest* UASInventoryViewModel::GetLootChest() const
{
	return Inventory.IsValid() ? Cast<AASLootChest>(Inventory->GetOwner()) : nullptr;
}

UASItemSlotViewModel* UASInventoryViewModel::GetSlot(int32 Index)
{
	if (const auto* Existing = SlotModels.Find(Index)) return *Existing;
	if (!IsExposedSlot(Index)) return nullptr;
	UASItemSlotViewModel* Slot = NewObject<UASItemSlotViewModel>(this);
	SlotModels.Add(Index, Slot);
	Slot->Initialize(this, Index);
	return Slot;
}

void UASInventoryViewModel::Refresh()
{
	FASInventoryPresentation Value;
	Value.Title = NSLOCTEXT("ASInventory", "LootTitle", "보급품 상자");
	TArray<TObjectPtr<UASItemSlotViewModel>> Quick, Storage, Container;
	if (Inventory.IsValid())
	{
		Value.StorageSlotCount = Inventory->GetStorageSlotCount();
		Value.ItemCount = Inventory->GetItems().Num();
		if (GetLootChest())
		{
			Value.ItemCount = 0;
			for (const UASItemInstance* Item : Inventory->GetItems()) if (IsLootItemVisible(Item)) ++Value.ItemCount;
		}
		Value.Capacity = FText::Format(NSLOCTEXT("ASInventory", "Capacity", "보관함 {0}칸 + 퀵슬롯 10칸  ·  사용 중 {1}칸"),
			FText::AsNumber(Value.StorageSlotCount), FText::AsNumber(Inventory->GetUsedSlotCount()));
		if (IsPlayerSource())
		{
			for (int32 I = 0; I < UASInventoryComponent::QuickSlotCount; ++I) Quick.Add(GetSlot(I));
			for (int32 I = 0; I < Value.StorageSlotCount; ++I) Storage.Add(GetSlot(UASInventoryComponent::QuickSlotCount + I));
			GetSlot(UASInventoryComponent::WeaponSlotIndex); GetSlot(UASInventoryComponent::BagSlotIndex);
		}
		else
		{
			int32 HighestSlot = -1;
			for (const UASItemInstance* Item : Inventory->GetItems()) HighestSlot = FMath::Max(HighestSlot, Item->GetInventorySlot());
			const int32 Count = Inventory->GetMaxSlots() > 0 ? Inventory->GetMaxSlots() : FMath::Max(12, FMath::DivideAndRoundUp(HighestSlot + 1, 4) * 4);
			for (int32 I = 0; I < Count; ++I) Container.Add(GetSlot(I));
		}
		if (const AASLootChest* Chest = GetLootChest())
		{
			switch (Chest->GetLootRarity())
			{
			case EASLootRarity::Uncommon: Value.Title = NSLOCTEXT("ASInventory", "UncommonChest", "고급 보급품 상자"); Value.TitleColor = FLinearColor(0.35f, 0.78f, 0.46f); break;
			case EASLootRarity::Rare: Value.Title = NSLOCTEXT("ASInventory", "RareChest", "희귀 보급품 상자"); Value.TitleColor = FLinearColor(0.68f, 0.49f, 0.91f); break;
			case EASLootRarity::Epic: Value.Title = NSLOCTEXT("ASInventory", "EpicChest", "영웅 보급품 상자"); Value.TitleColor = FLinearColor(0.72f, 0.39f, 0.91f); break;
			case EASLootRarity::Legendary: Value.Title = NSLOCTEXT("ASInventory", "LegendaryChest", "전설 보급품 상자"); Value.TitleColor = FLinearColor(0.96f, 0.60f, 0.28f); break;
			}
			if (!Chest->GetLootDisplayName().IsEmpty()) Value.Title = Chest->GetLootDisplayName();
		}
	}
	for (auto It = SlotModels.CreateIterator(); It; ++It)
	{
		if (!IsExposedSlot(It.Key()) || (!IsPlayerSource() && !Container.Contains(It.Value()))) { It.Value()->Release(); It.RemoveCurrent(); }
		else It.Value()->Refresh();
	}
	// Publish layout after slot snapshots are current. Slot identities stay stable across mutations.
	UE_MVVM_SET_PROPERTY_VALUE(QuickSlots, Quick);
	UE_MVVM_SET_PROPERTY_VALUE(StorageSlots, Storage);
	UE_MVVM_SET_PROPERTY_VALUE(ContainerSlots, Container);
	UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
}

bool UASInventoryViewModel::MatchesSlot(int32 Slot, FGuid Expected) const
{
	if (!IsExposedSlot(Slot)) return false;
	const UASItemInstance* Item = Inventory->GetItemAtSlot(Slot);
	return (Item ? Item->GetInstanceId() : FGuid()) == Expected;
}

void UASInventoryViewModel::ActivateSlot(int32 Slot, FGuid Expected)
{
	if (IsPlayerSource() && Interaction.IsValid() && Expected.IsValid() && MatchesSlot(Slot, Expected)) Interaction->RequestActivateSlot(Slot, Expected);
}

void UASInventoryViewModel::DropSlot(int32 Slot, FGuid Expected, bool bSingle)
{
	if (!IsPlayerSource() || !Interaction.IsValid() || !Expected.IsValid() || !MatchesSlot(Slot, Expected)) return;
	if (bSingle) Interaction->RequestDropOneFromSlot(Slot, Expected);
	else Interaction->RequestDropSlot(Slot, Expected);
}

void UASInventoryViewModel::MoveSlot(int32 From, int32 To, FGuid ExpectedFrom, FGuid ExpectedTo)
{
	if (!IsPlayerSource() || !Interaction.IsValid() || !ExpectedFrom.IsValid() || From == To
		|| !MatchesSlot(From, ExpectedFrom) || !MatchesSlot(To, ExpectedTo)) return;
	if (To == UASInventoryComponent::WeaponSlotIndex)
	{
		const UASItemSlotViewModel* Source = GetSlot(From);
		if (Source && Source->Presentation.ItemType == EASItemType::Weapon) ActivateSlot(From, ExpectedFrom);
		return;
	}
	Interaction->RequestSwapSlots(From, To, ExpectedFrom, ExpectedTo);
}

void UASInventoryViewModel::LootItem(FGuid Expected)
{
	UASItemInstance* Item = Inventory.IsValid() ? Inventory->FindItemById(Expected) : nullptr;
	if (Interaction.IsValid() && GetLootChest() && Item) Interaction->RequestLoot(GetLootChest(), Expected, Item->GetQuantity());
}

void UASInventoryViewModel::DropLootItem(FGuid Expected)
{
	if (Interaction.IsValid() && GetLootChest() && Inventory->FindItemById(Expected)) Interaction->RequestDropLoot(GetLootChest(), Expected);
}

void UASInventoryViewModel::CloseInventory() { if (Interaction.IsValid()) Interaction->CloseInventory(); }
void UASInventoryViewModel::OpenCrafting()
{
	if (AASPlayerController* PC = Cast<AASPlayerController>(Player.Get())) { CloseInventory(); PC->ToggleCrafting(); }
}
void UASInventoryViewModel::HandleActionFailed(FText Message)
{
	FASInventoryPresentation Value = Presentation; Value.Feedback = Message;
	UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
}
void UASInventoryViewModel::ClearFeedback() { HandleActionFailed(FText::GetEmpty()); }

bool UASInventoryViewModel::IsLootItemVisible(const UASItemInstance* Item) const
{
	if (!Item || !Item->GetItemDefinition()) return false;
	const UASItemDefinition* Definition = Item->GetItemDefinition();
	return Definition->ItemType != EASItemType::Quest
		|| (Quests.IsValid() && Quests->GetCollectibleQuestItemCount(Definition) > 0);
}
