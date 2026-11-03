#include "Items/Interaction/ASItemInteractionComponent.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Core/Framework/ASGameState.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/World.h"
#include "GAS/ASAbilityTags.h"
#include "Items/Crafting/ASCraftingComponent.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/UI/ASInventoryBarWidget.h"
#include "Items/UI/ASInventoryPanelWidget.h"

bool UASItemInteractionComponent::IsInventoryOpen() const
{
	return InventoryPanel && InventoryPanel->IsInViewport();
}

void UASItemInteractionComponent::ToggleInventory()
{
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController() || PC->IsInGameMenuOpen()) return;
	if (IsInventoryOpen()) { CloseInventory(); return; }
	if (!PC->GetPawn() || !GetPlayerInventory()) return;
	if (const AASGameState* State = GetWorld()->GetGameState<AASGameState>(); State && State->IsGameEnded()) return;
	UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(GetPlayerInventory(), PC->GetPawn());
	if (!ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)) return;
	PC->CancelMovementForCommittedAbility();
	CancelPendingInteraction();
	ASC->LocalInputCancel();
	if (UWidgetBlueprintLibrary::IsDragDropping()) UWidgetBlueprintLibrary::CancelDragDrop();
	CloseLoot();
	PC->GetCraftingComponent()->CloseCrafting();
	if (!InventoryPanel)
		InventoryPanel = CreateWidget<UASInventoryPanelWidget>(PC,
			InventoryPanelClass ? InventoryPanelClass.Get() : UASInventoryPanelWidget::StaticClass());
	if (!InventoryPanel) return;
	InventoryPanel->SetInventory(GetPlayerInventory());
	InventoryPanel->AddToViewport(14);
	if (InventoryBar) InventoryBar->SetVisibility(ESlateVisibility::Collapsed);
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(InventoryPanel->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
}

void UASItemInteractionComponent::CloseInventory()
{
	if (!IsInventoryOpen()) return;
	if (UWidgetBlueprintLibrary::IsDragDropping()) UWidgetBlueprintLibrary::CancelDragDrop();
	InventoryPanel->RemoveFromParent();
	if (InventoryBar) InventoryBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	AASPlayerController* PC = GetASController();
	if (!PC || !PC->IsLocalController() || !GetWorld() || GetWorld()->bIsTearingDown || PC->IsInGameMenuOpen()) return;
	if (const AASGameState* State = GetWorld()->GetGameState<AASGameState>(); State && State->IsGameEnded()) return;
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
	UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

void UASItemInteractionComponent::ClientInventoryActionFailed_Implementation(const FText& Message)
{
	OnInventoryActionFailed.Broadcast(Message);
}
