#include "Items/Interaction/ASItemInteractionComponent.h"

#include "Blueprint/UserWidget.h"
#include "Core/Framework/ASPlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/ASInteractable.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/ASItemInstance.h"
#include "Items/Loot/ASLootChest.h"
#include "Items/Loot/ASDroppedItem.h"
#include "Items/UI/ASLootWidget.h"
#include "Items/UI/ASInventoryBarWidget.h"
#include "Items/UI/ASInventoryPanelWidget.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASTargetedItemUseAbility.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Skills/ASSkillTags.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	bool IsDeadPawn(const AASPlayerController* PC)
	{
		const AASPlayerCharacter* Pawn = PC ? Cast<AASPlayerCharacter>(PC->GetPawn()) : nullptr;
		return Pawn && Pawn->IsDead();
	}
}

UASItemInteractionComponent::UASItemInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

AASPlayerController* UASItemInteractionComponent::GetASController() const
{
	return Cast<AASPlayerController>(GetOwner());
}

UASInventoryComponent* UASItemInteractionComponent::GetPlayerInventory() const
{
	const AASPlayerController* PC = GetASController();
	if (!PC || !IsValid(PC->GetPawn())) return nullptr;
	if (UASInventoryComponent* Inventory = PC->GetInventoryComponent()) return Inventory;
	if (UASInventoryComponent* Inventory = PC->GetPawn()->FindComponentByClass<UASInventoryComponent>()) return Inventory;
	return PC->PlayerState ? PC->PlayerState->FindComponentByClass<UASInventoryComponent>() : nullptr;
}

bool UASItemInteractionComponent::CanReachTarget(AActor* Target) const
{
	const AASPlayerController* PC = GetASController();
	if (!PC || !IsValid(PC->GetPawn()) || !IsValid(Target)
		|| !Target->Implements<UASInteractable>()) return false;
	const float Range = IASInteractable::Execute_GetInteractionRange(Target);
	return Range >= 0.0f && FVector::DistSquared(PC->GetPawn()->GetActorLocation(),
		IASInteractable::Execute_GetInteractionLocation(Target)) <= FMath::Square(Range);
}

bool UASItemInteractionComponent::TryInteractWith(AActor* Target)
{
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController() || !IsValid(PC->GetPawn()) || !IsValid(Target)
		|| !Target->Implements<UASInteractable>() || Target->IsA<AASDroppedItem>()) return false;
	CloseLoot();
	PendingTarget = Target;
	PendingStartedAt = GetWorld()->GetTimeSeconds();
	if (CanReachTarget(Target))
	{
		PC->StopMovement();
		ServerInteract(Target);
		CancelPendingInteraction();
	}
	else PC->RequestMoveTo(IASInteractable::Execute_GetInteractionLocation(Target));
	return true;
}

bool UASItemInteractionComponent::RequestPickupDroppedItem(AASDroppedItem* Item)
{
	AASPlayerController* PC = GetASController();
	if (!bDroppedItemNamesVisible || !PC || !PC->IsLocalController() || IsDeadPawn(PC) || !IsValid(PC->GetPawn())
		|| !IsValid(Item) || !Item->GetDroppedItem() || !GetPlayerInventory()) return false;
	CloseLoot();
	PendingTarget = Item;
	PendingStartedAt = GetWorld()->GetTimeSeconds();
	if (CanReachTarget(Item))
	{
		PC->StopMovement();
		ServerInteract(Item);
		CancelPendingInteraction();
	}
	else PC->RequestMoveTo(Item->GetActorLocation());
	return true;
}

bool UASItemInteractionComponent::IsCursorOverInventory() const
{
	return (IsValid(InventoryPanel) && InventoryPanel->IsCursorOverInventory())
		|| (IsValid(InventoryBar) && InventoryBar->IsCursorOverInventory());
}

void UASItemInteractionComponent::HandleWorldRightClick()
{
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController() || IsDeadPawn(PC) || !IsValid(PC->GetPawn())
		|| IsInventoryOpen() || IsCursorOverInventory()) return;
	if (UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(GetPlayerInventory(), PC->GetPawn());
		ASC && ASC->HasMatchingGameplayTag(ASGameplayTags::State_Targeting))
	{
		ASC->LocalInputCancel();
	}
	CancelPendingInteraction();
	FVector Destination;
	if (PC->GetCursorWorldLocation(Destination)) PC->RequestMoveTo(Destination);
}

void UASItemInteractionComponent::CancelPendingInteraction()
{
	PendingTarget.Reset();
}

void UASItemInteractionComponent::SetHoveredDroppedItemLabel(AASDroppedItem* Item, const bool bHovered)
{
	if (bHovered && bDroppedItemNamesVisible) HoveredDroppedItemLabel = Item;
	else if (HoveredDroppedItemLabel.Get() == Item) HoveredDroppedItemLabel.Reset();
}

void UASItemInteractionComponent::ToggleDroppedItemNames()
{
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController() || PC->IsInGameMenuOpen()) return;

	bDroppedItemNamesVisible = !bDroppedItemNamesVisible;
	if (!bDroppedItemNamesVisible)
	{
		if (PendingTarget.IsValid() && PendingTarget->IsA<AASDroppedItem>())
		{
			PC->StopMovement();
			CancelPendingInteraction();
		}
		if (HoveredDroppedItem.IsValid()) HoveredDroppedItem->SetWorldHovered(false);
		HoveredDroppedItem.Reset();
		HoveredDroppedItemLabel.Reset();
	}

	for (TActorIterator<AASDroppedItem> It(GetWorld()); It; ++It)
	{
		It->RefreshNameVisibility();
	}
	ServerSetDroppedItemNamesVisible(bDroppedItemNamesVisible);
}

void UASItemInteractionComponent::ServerSetDroppedItemNamesVisible_Implementation(const bool bVisible)
{
	bDroppedItemNamesVisible = bVisible;
}

void UASItemInteractionComponent::UpdateDroppedItemHover()
{
	AASDroppedItem* Next = nullptr;
	AASPlayerController* PC = GetASController();
	if (bDroppedItemNamesVisible && PC && IsValid(PC->GetPawn()) && PC->bShowMouseCursor && !IsDeadPawn(PC)
		&& !IsCursorOverInventory() && !IsLootOpen() && !IsInventoryOpen()
		&& FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsDragDropping())
	{
		if (HoveredDroppedItemLabel.IsValid() && HoveredDroppedItemLabel->GetDroppedItem())
			Next = HoveredDroppedItemLabel.Get();
	}
	if (HoveredDroppedItem.Get() != Next)
	{
		if (HoveredDroppedItem.IsValid()) HoveredDroppedItem->SetWorldHovered(false);
		HoveredDroppedItem = Next;
		if (Next) Next->SetWorldHovered(true);
	}
}

void UASItemInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController()) return;
	InitializeInventoryBar();
	if (IsInventoryOpen() && (!PC->GetPawn() || IsDeadPawn(PC))) CloseInventory();
	UpdateDroppedItemHover();
	if (InventoryBar && InventoryBar->GetInventory() != GetPlayerInventory())
		InventoryBar->SetInventory(GetPlayerInventory());
	if (PendingTarget.IsValid())
	{
		if (!IsValid(PC->GetPawn()) || GetWorld()->GetTimeSeconds() - PendingStartedAt > 10.0)
		{
			CancelPendingInteraction();
		}
		else if (CanReachTarget(PendingTarget.Get()))
		{
			PC->StopMovement();
			ServerInteract(PendingTarget.Get());
			CancelPendingInteraction();
		}
	}
	if (IsLootOpen() && (!CanReachTarget(OpenChest.Get()) || LootPawn.Get() != PC->GetPawn())) CloseLoot();
}

void UASItemInteractionComponent::ServerInteract_Implementation(AActor* Target)
{
	AASPlayerController* PC = GetASController();
	if (IsValid(Target) && Target->IsA<AASDroppedItem>() && !bDroppedItemNamesVisible) return;
	if (!CanReachTarget(Target) || !IASInteractable::Execute_CanInteract(Target, PC->GetPawn())) return;
	if (Target->IsA<AASDroppedItem>())
	{
		IASInteractable::Execute_Interact(Target, PC->GetPawn());
		return;
	}
	AASLootChest* Chest = Cast<AASLootChest>(Target);
	if (Chest && !GetPlayerInventory()) return;
	IASInteractable::Execute_Interact(Target, PC->GetPawn());
	if (IsValid(Chest))
	{
		ServerOpenChest = Chest;
		ServerLootPawn = PC->GetPawn();
		ClientOpenLoot(Chest);
	}
}

void UASItemInteractionComponent::RequestLoot(AASLootChest* Chest, const FGuid InstanceId, const int32 Quantity)
{
	if (GetASController() && GetASController()->IsLocalController() && IsValid(Chest)
		&& InstanceId.IsValid() && Quantity > 0) ServerTakeLoot(Chest, InstanceId, Quantity);
}

void UASItemInteractionComponent::ServerTakeLoot_Implementation(AASLootChest* Chest, const FGuid InstanceId, const int32 Quantity)
{
	UASInventoryComponent* Inventory = GetPlayerInventory();
	if (!Inventory || Quantity <= 0 || !InstanceId.IsValid() || Chest != ServerOpenChest.Get()
		|| ServerLootPawn.Get() != GetASController()->GetPawn() || !CanReachTarget(Chest)) return;
	Chest->TakeLoot(GetASController()->GetPawn(), Inventory, InstanceId, Quantity);
}

void UASItemInteractionComponent::RequestDropLoot(AASLootChest* Chest, const FGuid InstanceId)
{
	AASPlayerController* PC = GetASController();
	if (PC && PC->IsLocalController() && IsLootOpen() && IsValid(Chest)
		&& Chest == OpenChest.Get() && InstanceId.IsValid()) ServerDropLoot(Chest, InstanceId);
}

void UASItemInteractionComponent::ServerDropLoot_Implementation(AASLootChest* Chest, const FGuid InstanceId)
{
	AASPlayerController* PC = GetASController();
	if (!PC || !GetPlayerInventory() || !IsValid(Chest) || !InstanceId.IsValid()
		|| Chest != ServerOpenChest.Get() || ServerLootPawn.Get() != PC->GetPawn() || !CanReachTarget(Chest)) return;
	Chest->DropLoot(PC->GetPawn(), InstanceId);
}

void UASItemInteractionComponent::RequestUseItem(const FGuid InstanceId)
{
	if (GetASController() && GetASController()->IsLocalController() && InstanceId.IsValid()) ServerUseItem(InstanceId);
}

void UASItemInteractionComponent::ServerUseItem_Implementation(const FGuid InstanceId)
{
	if (UASInventoryComponent* Inventory = GetPlayerInventory()) Inventory->UseItem(InstanceId, GetASController()->GetPawn());
}

void UASItemInteractionComponent::RequestEquipItem(const FGuid InstanceId)
{
	if (GetASController() && GetASController()->IsLocalController() && InstanceId.IsValid()) ServerEquipItem(InstanceId);
}

void UASItemInteractionComponent::ServerEquipItem_Implementation(const FGuid InstanceId)
{
	if (UASInventoryComponent* Inventory = GetPlayerInventory()) Inventory->EquipItem(InstanceId, GetASController()->GetPawn());
}

void UASItemInteractionComponent::RequestUnequipItem()
{
	if (GetASController() && GetASController()->IsLocalController()) ServerUnequipItem();
}

void UASItemInteractionComponent::ServerUnequipItem_Implementation()
{
	if (UASInventoryComponent* Inventory = GetPlayerInventory()) Inventory->UnequipItem();
}

void UASItemInteractionComponent::InitializeInventoryBar()
{
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalPlayerController()) return;
	if (!InventoryInput)
	{
		InventoryInput = NewObject<UInputComponent>(PC, TEXT("InventorySlotInput"));
		InventoryInput->RegisterComponent();
		InventoryInput->Priority = 1;
		const FKey Keys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
			EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
		for (int32 Index = 0; Index < UASInventoryComponent::PlayerSlotCount; ++Index)
		{
			FInputKeyBinding Binding(FInputChord(Keys[Index]), IE_Pressed);
			Binding.bConsumeInput = true;
			Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, Index]() { ActivateSlotFromKey(Index); });
			InventoryInput->KeyBindings.Add(Binding);
		}
		FInputKeyBinding RightClick(FInputChord(EKeys::RightMouseButton), IE_Pressed);
		RightClick.bConsumeInput = true;
		RightClick.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ThisClass::HandleWorldRightClick);
		InventoryInput->KeyBindings.Add(RightClick);
		FInputKeyBinding ToggleNames(FInputChord(EKeys::Z), IE_Pressed);
		ToggleNames.bConsumeInput = true;
		ToggleNames.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ThisClass::ToggleDroppedItemNames);
		InventoryInput->KeyBindings.Add(ToggleNames);
		PC->PushInputComponent(InventoryInput);
	}
	if (!InventoryBar)
	{
		InventoryBar = CreateWidget<UASInventoryBarWidget>(PC,
			InventoryBarClass ? InventoryBarClass.Get() : UASInventoryBarWidget::StaticClass());
		if (InventoryBar)
		{
			InventoryBar->SetInventory(GetPlayerInventory());
			InventoryBar->AddToViewport(10);
		}
	}
}

void UASItemInteractionComponent::ActivateSlotFromKey(const int32 SlotIndex)
{
	if (IsInventoryOpen() || (GetASController() && GetASController()->IsInGameMenuOpen())) return;
	UASInventoryComponent* Inventory = GetPlayerInventory();
	UASItemInstance* Item = Inventory ? Inventory->GetItemAtSlot(SlotIndex) : nullptr;
	if (Item) RequestActivateSlot(SlotIndex, Item->GetInstanceId());
}

void UASItemInteractionComponent::RequestActivateSlot(const int32 SlotIndex, const FGuid ExpectedItem)
{
	UASInventoryComponent* Inventory = GetPlayerInventory();
	if (GetASController() && GetASController()->IsLocalController() && Inventory && ExpectedItem.IsValid()
		&& (Inventory->IsValidItemSlot(SlotIndex) || SlotIndex == UASInventoryComponent::WeaponSlotIndex
			|| SlotIndex == UASInventoryComponent::BagSlotIndex))
	{
		UASItemInstance* Item = Inventory->GetItemAtSlot(SlotIndex);
		UClass* UseClass = Item ? Item->GetItemDefinition()->UseAbility.Get() : nullptr;
		if (Item && Item->GetInstanceId() == ExpectedItem && UseClass && UseClass->IsChildOf(UASTargetedItemUseAbility::StaticClass()))
			CloseInventory();
		ServerActivateSlot(SlotIndex, ExpectedItem);
	}
}

void UASItemInteractionComponent::ServerActivateSlot_Implementation(const int32 SlotIndex, const FGuid ExpectedItem)
{
	UASInventoryComponent* Inventory = GetPlayerInventory();
	if (!Inventory || (!Inventory->IsValidItemSlot(SlotIndex) && SlotIndex != UASInventoryComponent::WeaponSlotIndex
		&& SlotIndex != UASInventoryComponent::BagSlotIndex)) return;
	UASItemInstance* Item = Inventory->GetItemAtSlot(SlotIndex);
	if (!Item || Item->GetInstanceId() != ExpectedItem) return;
	if (!Inventory->UseItem(ExpectedItem, GetASController()->GetPawn()) && Item->GetItemDefinition()->ItemType == EASItemType::Bag)
		ClientInventoryActionFailed(NSLOCTEXT("ASInventory", "BagChangeFailed", "??? ??? ? ????. ?? ??? ??? ??? ?????."));
}

void UASItemInteractionComponent::RequestSwapSlots(const int32 FromSlot, const int32 ToSlot,
	const FGuid ExpectedFrom, const FGuid ExpectedTo)
{
	UASInventoryComponent* Inventory = GetPlayerInventory();
	if (GetASController() && GetASController()->IsLocalController() && Inventory && ExpectedFrom.IsValid()
		&& (Inventory->IsValidItemSlot(FromSlot) || FromSlot == UASInventoryComponent::BagSlotIndex)
		&& (Inventory->IsValidItemSlot(ToSlot) || ToSlot == UASInventoryComponent::BagSlotIndex))
		ServerSwapSlots(FromSlot, ToSlot, ExpectedFrom, ExpectedTo);
}

void UASItemInteractionComponent::ServerSwapSlots_Implementation(const int32 FromSlot, const int32 ToSlot,
	const FGuid ExpectedFrom, const FGuid ExpectedTo)
{
	if (IsDeadPawn(GetASController())) return;
	if (UASInventoryComponent* Inventory = GetPlayerInventory())
		if (!Inventory->SwapSlots(FromSlot, ToSlot, ExpectedFrom, ExpectedTo))
			ClientInventoryActionFailed(NSLOCTEXT("ASInventory", "MoveFailed", "???? ??? ? ????. ?? ??? ??? ?????."));
}

void UASItemInteractionComponent::RequestDropSlot(const int32 SlotIndex, const FGuid ExpectedItem)
{
	UASInventoryComponent* Inventory = GetPlayerInventory();
	if (GetASController() && GetASController()->IsLocalController() && ExpectedItem.IsValid()
		&& Inventory && Inventory->IsValidItemSlot(SlotIndex)) ServerDropSlot(SlotIndex, ExpectedItem);
}

void UASItemInteractionComponent::RequestDropOneFromSlot(const int32 SlotIndex, const FGuid ExpectedItem)
{
	UASInventoryComponent* Inventory = GetPlayerInventory();
	if (GetASController() && GetASController()->IsLocalController() && ExpectedItem.IsValid()
		&& Inventory && Inventory->IsValidItemSlot(SlotIndex)) ServerDropOneFromSlot(SlotIndex, ExpectedItem);
}

void UASItemInteractionComponent::ServerDropOneFromSlot_Implementation(const int32 SlotIndex, const FGuid ExpectedItem)
{
	if (UASInventoryComponent* Inventory = GetPlayerInventory())
		Inventory->DropOneFromSlot(SlotIndex, ExpectedItem, GetASController()->GetPawn());
}

void UASItemInteractionComponent::ServerDropSlot_Implementation(const int32 SlotIndex, const FGuid ExpectedItem)
{
	if (UASInventoryComponent* Inventory = GetPlayerInventory())
		Inventory->DropSlot(SlotIndex, ExpectedItem, GetASController()->GetPawn());
}

void UASItemInteractionComponent::ClientOpenLoot_Implementation(AASLootChest* Chest)
{
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController() || !IsValid(Chest) || !GetPlayerInventory()) return;
	CloseInventory();
	// Remove the previous widget without sending a close RPC for the newly accepted server context.
	if (IsValid(LootWidget))
	{
		LootWidget->OnLootClosed.RemoveDynamic(this, &UASItemInteractionComponent::HandleLootClosed);
		LootWidget->RemoveFromParent();
	}
	TSubclassOf<UASLootWidget> ActualClass = LootWidgetClass;
	if (!ActualClass) ActualClass = UASLootWidget::StaticClass();
	LootWidget = CreateWidget<UASLootWidget>(PC, ActualClass);
	if (!IsValid(LootWidget)) return;
	OpenChest = Chest;
	LootPawn = PC->GetPawn();
	LootWidget->SetLootContext(Chest->GetLootInventory(), GetPlayerInventory());
	LootWidget->OnLootClosed.AddUniqueDynamic(this, &UASItemInteractionComponent::HandleLootClosed);
	LootWidget->AddToViewport();
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(LootWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
}

bool UASItemInteractionComponent::IsLootOpen() const
{
	return IsValid(LootWidget) && LootWidget->IsInViewport();
}

void UASItemInteractionComponent::CloseLoot()
{
	if (IsValid(LootWidget)) LootWidget->RemoveFromParent();
}

void UASItemInteractionComponent::HandleLootClosed()
{
	LootWidget = nullptr;
	OpenChest.Reset();
	LootPawn.Reset();
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController()) return;
	ServerCloseLoot();
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
}

void UASItemInteractionComponent::ServerCloseLoot_Implementation()
{
	ServerOpenChest.Reset();
	ServerLootPawn.Reset();
}

void UASItemInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseInventory();
	InventoryPanel = nullptr;
	if (HoveredDroppedItem.IsValid()) HoveredDroppedItem->SetWorldHovered(false);
	HoveredDroppedItem.Reset();
	HoveredDroppedItemLabel.Reset();
	if (InventoryInput)
	{
		if (AASPlayerController* PC = GetASController()) PC->PopInputComponent(InventoryInput);
		InventoryInput->DestroyComponent();
		InventoryInput = nullptr;
	}
	if (InventoryBar)
	{
		InventoryBar->RemoveFromParent();
		InventoryBar = nullptr;
	}
	if (IsValid(LootWidget))
	{
		LootWidget->OnLootClosed.RemoveDynamic(this, &UASItemInteractionComponent::HandleLootClosed);
		LootWidget->RemoveFromParent();
		LootWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
