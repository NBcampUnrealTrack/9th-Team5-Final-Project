#include "Core/UI/ASLobbyWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/Framework/ASLobbyPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Core/UI/ASLeaveRoomConfirmationWidget.h"
#include "Core/UI/ASUIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"

void UASLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (GetWorld()) GetWorld()->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
	HandleGameStateSet(GetLobbyState());
	if (ReadyButton) ReadyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleReady);
	if (StartGameButton) StartGameButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestStartGame);
	if (LeaveRoomButton) LeaveRoomButton->OnClicked.AddUniqueDynamic(this, &ThisClass::LeaveRoom);
	HandleLobbyUpdated();
}

void UASLobbyWidget::NativeDestruct()
{
	if (LeaveConfirmation) LeaveConfirmation->RemoveFromParent();
	if (GetWorld()) GetWorld()->GameStateSetEvent.RemoveAll(this);
	if (BoundState) BoundState->OnLobbyStateChanged.RemoveDynamic(this, &ThisClass::HandleLobbyUpdated);
	if (ReadyButton) ReadyButton->OnClicked.RemoveDynamic(this, &ThisClass::ToggleReady);
	if (StartGameButton) StartGameButton->OnClicked.RemoveDynamic(this, &ThisClass::RequestStartGame);
	if (LeaveRoomButton) LeaveRoomButton->OnClicked.RemoveDynamic(this, &ThisClass::LeaveRoom);
	Super::NativeDestruct();
}

void UASLobbyWidget::HandleGameStateSet(AGameStateBase* State)
{
	if (BoundState) BoundState->OnLobbyStateChanged.RemoveDynamic(this, &ThisClass::HandleLobbyUpdated);
	BoundState = Cast<AASLobbyGameState>(State);
	if (BoundState) BoundState->OnLobbyStateChanged.AddUniqueDynamic(this, &ThisClass::HandleLobbyUpdated);
	HandleLobbyUpdated();
}

AASLobbyGameState* UASLobbyWidget::GetLobbyState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AASLobbyGameState>() : nullptr;
}

bool UASLobbyWidget::IsLocalHost() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return Controller && Controller->HasAuthority() && Controller->IsLocalController();
}

void UASLobbyWidget::ToggleReady()
{
	AASLobbyPlayerController* Controller = GetOwningPlayer<AASLobbyPlayerController>();
	const AASLobbyGameState* State = GetLobbyState();
	if (!Controller || !Controller->PlayerState || !State || State->bGameStarting) return;
	const int32 LocalId = Controller->PlayerState->GetPlayerId();
	const FASLobbyPlayerInfo* Info = State->LobbyPlayers.FindByPredicate([LocalId](const FASLobbyPlayerInfo& Player)
		{ return Player.PlayerId == LocalId; });
	if (Info) Controller->SetReady(!Info->bIsReady);
}

void UASLobbyWidget::RequestStartGame()
{
	if (AASLobbyPlayerController* Controller = GetOwningPlayer<AASLobbyPlayerController>())
	{
		Controller->StartLobbyGame();
	}
}

void UASLobbyWidget::LeaveRoom()
{
	if (LeaveConfirmation && LeaveConfirmation->IsInViewport()) return;
	ULocalPlayer* Local = GetOwningLocalPlayer();
	UASUIManagerSubsystem* UI = Local ? Local->GetSubsystem<UASUIManagerSubsystem>() : nullptr;
	if (!UI) return;
	LeaveConfirmation = CreateWidget<UASLeaveRoomConfirmationWidget>(GetOwningPlayer());
	if (!LeaveConfirmation) return;
	LeaveConfirmation->SetReturnFocusWidget(this);
	LeaveConfirmation->OnConfirmed.BindUObject(this, &ThisClass::ConfirmLeaveRoom);
	UI->ShowScreen(TEXT("LeaveRoomConfirmation"), LeaveConfirmation, 200);
	SetIsEnabled(false);
	LeaveConfirmation->SetKeyboardFocus();
}

void UASLobbyWidget::ConfirmLeaveRoom()
{
	if (UGameInstance* Instance = GetGameInstance())
	{
		if (UASSessionSubsystem* Sessions = Instance->GetSubsystem<UASSessionSubsystem>())
		{
			if (LeaveRoomButton) LeaveRoomButton->SetIsEnabled(false);
			Sessions->LeaveRoom();
		}
	}
}

void UASLobbyWidget::HandleLobbyUpdated()
{
	const AASLobbyGameState* State = GetLobbyState();
	if (ReadyButton) ReadyButton->SetIsEnabled(State && !State->bGameStarting);
	if (StartGameButton) StartGameButton->SetIsEnabled(State && IsLocalHost() && State->bCanStartGame);
	if (StartGameButton) StartGameButton->SetVisibility(IsLocalHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (PlayerListText)
	{
		FString PlayerList;
		if (State)
		{
			for (const FASLobbyPlayerInfo& Player : State->LobbyPlayers)
			{
				PlayerList += FString::Printf(TEXT("%s%s : %s\n"), *Player.PlayerName,
					Player.bIsHost ? TEXT(" (방장)") : TEXT(""), Player.bIsReady ? TEXT("준비 완료") : TEXT("준비 중"));
			}
		}
		PlayerListText->SetText(FText::FromString(PlayerList));
	}
	if (LobbyStatusText)
	{
		LobbyStatusText->SetText(FText::FromString(State && State->bGameStarting ? TEXT("게임으로 이동 중입니다.")
			: State && State->bCanStartGame ? TEXT("전원 준비 완료. 방장이 게임을 시작할 수 있습니다.")
			: TEXT("참가 인원과 전원의 준비 완료를 기다리고 있습니다.")));
	}
	OnLobbyUpdated();
}
