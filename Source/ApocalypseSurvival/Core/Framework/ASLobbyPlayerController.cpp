#include "Core/Framework/ASLobbyPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Core/Framework/ASLobbyGameMode.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "Engine/World.h"
#include "Characters/Player/UI/ASCharacterSelectWidget.h"

void AASLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	GetWorld()->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
	if (LobbyWidgetClass)
	{
		// Reuse the screen if an existing BP BeginPlay already created it.
		TArray<UUserWidget*> ExistingWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, ExistingWidgets, LobbyWidgetClass, true);
		for (UUserWidget* ExistingWidget : ExistingWidgets)
		{
			if (ExistingWidget->GetOwningPlayer() != this) continue;
			if (!LobbyWidget) LobbyWidget = ExistingWidget;
			else ExistingWidget->RemoveFromParent();
		}
		if (!LobbyWidget) LobbyWidget = CreateWidget<UUserWidget>(this, LobbyWidgetClass);
		ShowWidget(LobbyWidget);
	}
	HandleGameStateSet(GetWorld()->GetGameState());
}

void AASLobbyPlayerController::ShowWidget(UUserWidget* Widget)
{
	if (!Widget)
	{
		return;
	}
	if (!Widget->IsInViewport()) Widget->AddToViewport();
	FInputModeUIOnly InputMode;
	if (Widget->IsFocusable()) InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AASLobbyPlayerController::HandleGameStateSet(AGameStateBase* State)
{
	if (BoundLobbyState) BoundLobbyState->OnLobbyStateChanged.RemoveDynamic(this, &ThisClass::HandleLobbyPhaseChanged);
	BoundLobbyState = Cast<AASLobbyGameState>(State);
	if (BoundLobbyState) BoundLobbyState->OnLobbyStateChanged.AddUniqueDynamic(this, &ThisClass::HandleLobbyPhaseChanged);
	HandleLobbyPhaseChanged();
}

void AASLobbyPlayerController::HandleLobbyPhaseChanged()
{
	if (!IsLocalController() || !BoundLobbyState
		|| BoundLobbyState->LobbyPhase != EASLobbyPhase::CharacterSelection || !BoundLobbyState->CharacterRoster) return;
	if (CharacterSelectWidget) return;
	if (!CharacterSelectWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterSelect] Set CharacterSelectWidgetClass on the lobby PlayerController."));
		return;
	}
	CharacterSelectWidget = CreateWidget<UASCharacterSelectWidget>(this, CharacterSelectWidgetClass);
	if (!CharacterSelectWidget) return;
	RemoveOwnedWidgets(LobbyWidgetClass);
	LobbyWidget = nullptr;
	ShowWidget(CharacterSelectWidget);
}

void AASLobbyPlayerController::RemoveOwnedWidgets(const TSubclassOf<UUserWidget> WidgetClass)
{
	if (!IsLocalController() || !WidgetClass) return;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, WidgetClass, true);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget->GetOwningPlayer() == this) Widget->RemoveFromParent();
	}
}

void AASLobbyPlayerController::ClearLobbyScreens()
{
	RemoveOwnedWidgets(LobbyWidgetClass);
	RemoveOwnedWidgets(CharacterSelectWidgetClass);
	if (LobbyWidget) LobbyWidget->RemoveFromParent();
	if (CharacterSelectWidget) CharacterSelectWidget->RemoveFromParent();
	LobbyWidget = nullptr;
	CharacterSelectWidget = nullptr;
}

void AASLobbyPlayerController::PreClientTravel(const FString& PendingURL, const ETravelType TravelType, const bool bIsSeamlessTravel)
{
	ClearLobbyScreens();
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void AASLobbyPlayerController::SelectLobbyCharacter(const int32 Index)
{
	if (IsLocalController()) ServerSelectLobbyCharacter(Index);
}

void AASLobbyPlayerController::SetCharacterReady(const bool bReady)
{
	if (IsLocalController()) ServerSetCharacterReady(bReady);
}

void AASLobbyPlayerController::ServerSelectLobbyCharacter_Implementation(const int32 Index)
{
	if (AASLobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AASLobbyGameMode>()) Mode->SelectCharacter(this, Index);
}

void AASLobbyPlayerController::ServerSetCharacterReady_Implementation(const bool bReady)
{
	if (AASLobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AASLobbyGameMode>()) Mode->SetCharacterReady(this, bReady);
}

void AASLobbyPlayerController::InitializeCharacterSelection(const int32 DefaultIndex)
{
	if (HasAuthority()) SetCharacterSelectionOnServer(DefaultIndex, false);
}

void AASLobbyPlayerController::SetCharacterSelectionOnServer(const int32 Index, const bool bReady)
{
	if (!HasAuthority()) return;
	CharacterIndexOnServer = Index;
	bCharacterReadyOnServer = bReady;
}

void AASLobbyPlayerController::SetReady(const bool bReady)
{
	if (IsLocalController()) ServerSetReady(bReady);
}

void AASLobbyPlayerController::StartLobbyGame()
{
	if (IsLocalController()) ServerStartLobbyGame();
}

void AASLobbyPlayerController::ServerSetReady_Implementation(const bool bReady)
{
	AASLobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AASLobbyGameMode>();
	const AASLobbyGameState* State = GetWorld()->GetGameState<AASLobbyGameState>();
	if (!Mode || !State || State->bGameStarting) return;
	bReadyOnServer = bReady;
	UE_LOG(LogTemp, Log, TEXT("[Lobby] Ready=%d: %s"), bReady, *GetName());
	Mode->RefreshLobby();
}

void AASLobbyPlayerController::ServerStartLobbyGame_Implementation()
{
	if (AASLobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AASLobbyGameMode>())
	{
		Mode->TryStartGame(this);
	}
}

void AASLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GameStateSetEvent.RemoveAll(this);
	if (BoundLobbyState) BoundLobbyState->OnLobbyStateChanged.RemoveDynamic(this, &ThisClass::HandleLobbyPhaseChanged);
	ClearLobbyScreens();

	Super::EndPlay(EndPlayReason);
}
