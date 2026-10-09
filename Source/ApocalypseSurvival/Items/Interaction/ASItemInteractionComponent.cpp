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
#include "Items/Data/ASItemDefinition.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Characters/Player/ASPlayerCharacter.h"

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
	if (!PC || !PC->IsLocalController() || IsDeadPawn(PC) || !IsValid(PC->GetPawn())
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

void UASItemInteractionComponent::HandleWorldRightClick()
{
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController() || IsDeadPawn(PC) || !IsValid(PC->GetPawn())) return;
	FHitResult Hit;
	if (PC->GetHitResultUnderCursor(ECC_Visibility, true, Hit))
	{
		if (AASDroppedItem* Item = Cast<AASDroppedItem>(Hit.GetActor()))
		{
			RequestPickupDroppedItem(Item);
			return;
		}
	}
	CancelPendingInteraction();
	FVector Destination;
	if (PC->GetCursorWorldLocation(Destination)) PC->RequestMoveTo(Destination);
}

void UASItemInteractionComponent::CancelPendingInteraction()
{
	PendingTarget.Reset();
}

void UASItemInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController()) return;
	InitializeInventoryBar();
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
	UASInventoryComponent* Inventory = GetPlayerInventory();
	UASItemInstance* Item = Inventory ? Inventory->GetItemAtSlot(SlotIndex) : nullptr;
	if (Item) RequestActivateSlot(SlotIndex, Item->GetInstanceId());
}

void UASItemInteractionComponent::RequestActivateSlot(const int32 SlotIndex, const FGuid ExpectedItem)
{
	if (GetASController() && GetASController()->IsLocalController() && ExpectedItem.IsValid()
		&& SlotIndex >= 0 && SlotIndex < UASInventoryComponent::PlayerSlotCount)
		ServerActivateSlot(SlotIndex, ExpectedItem);
}

void UASItemInteractionComponent::ServerActivateSlot_Implementation(const int32 SlotIndex, const FGuid ExpectedItem)
{
	UASInventoryComponent* Inventory = GetPlayerInventory();
	if (!Inventory || SlotIndex < 0 || SlotIndex >= UASInventoryComponent::PlayerSlotCount) return;
	UASItemInstance* Item = Inventory->GetItemAtSlot(SlotIndex);
	if (!Item || Item->GetInstanceId() != ExpectedItem) return;
	Inventory->UseItem(ExpectedItem, GetASController()->GetPawn());
}

void UASItemInteractionComponent::RequestSwapSlots(const int32 FromSlot, const int32 ToSlot,
	const FGuid ExpectedFrom, const FGuid ExpectedTo)
{
	if (GetASController() && GetASController()->IsLocalController() && ExpectedFrom.IsValid()
		&& FromSlot >= 0 && FromSlot < UASInventoryComponent::PlayerSlotCount
		&& ToSlot >= 0 && ToSlot < UASInventoryComponent::PlayerSlotCount)
		ServerSwapSlots(FromSlot, ToSlot, ExpectedFrom, ExpectedTo);
}

void UASItemInteractionComponent::ServerSwapSlots_Implementation(const int32 FromSlot, const int32 ToSlot,
	const FGuid ExpectedFrom, const FGuid ExpectedTo)
{
	if (UASInventoryComponent* Inventory = GetPlayerInventory())
		Inventory->SwapSlots(FromSlot, ToSlot, ExpectedFrom, ExpectedTo);
}

void UASItemInteractionComponent::RequestDropSlot(const int32 SlotIndex, const FGuid ExpectedItem)
{
	if (GetASController() && GetASController()->IsLocalController() && ExpectedItem.IsValid()
		&& SlotIndex >= 0 && SlotIndex < UASInventoryComponent::PlayerSlotCount)
		ServerDropSlot(SlotIndex, ExpectedItem);
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
