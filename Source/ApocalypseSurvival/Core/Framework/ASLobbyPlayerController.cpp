#include "Core/Framework/ASLobbyPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Core/Framework/ASLobbyGameMode.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "Engine/World.h"

void AASLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (!LobbyWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lobby] LobbyWidgetClass를 지정해 주세요."));
		return;
	}

	LobbyWidget = CreateWidget<UUserWidget>(this, LobbyWidgetClass);
	if (!LobbyWidget)
	{
		return;
	}

	LobbyWidget->AddToViewport();
	FInputModeUIOnly InputMode;
	if (LobbyWidget->IsFocusable()) InputMode.SetWidgetToFocus(LobbyWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
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
	if (LobbyWidget)
	{
		LobbyWidget->RemoveFromParent();
		LobbyWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
