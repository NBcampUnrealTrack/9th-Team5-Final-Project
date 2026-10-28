#include "Core/UI/ASLobbyWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Core/Framework/ASLobbyPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Core/UI/ASLeaveRoomConfirmationWidget.h"

void UASLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!InviteFriendsButton)
	{
		if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(GetRootWidget()))
		{
			InviteFriendsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InviteFriendsButton"));
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
			Label->SetText(FText::FromString(TEXT("Steam 친구 초대")));
			Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
			InviteFriendsButton->SetContent(Label);
			UCanvasPanelSlot* InviteSlot = Canvas->AddChildToCanvas(InviteFriendsButton);
			InviteSlot->SetAnchors(FAnchors(1, 0));
			InviteSlot->SetAlignment(FVector2D(1, 0));
			InviteSlot->SetPosition(FVector2D(-24, 24));
			InviteSlot->SetSize(FVector2D(220, 48));
			InviteSlot->SetZOrder(10);
		}
	}
	if (InviteFriendsButton) InviteFriendsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::InviteFriends);
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
	if (InviteFriendsButton) InviteFriendsButton->OnClicked.RemoveDynamic(this, &ThisClass::InviteFriends);
	Super::NativeDestruct();
}

void UASLobbyWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!InviteFriendsButton) return;
	const UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>();
	const AASLobbyGameState* State = GetLobbyState();
	InviteFriendsButton->SetIsEnabled(Sessions && Sessions->CanInviteFriends() && State && !State->bGameStarting);
	InviteFriendsButton->SetToolTipText(FText::FromString(Sessions && Sessions->IsSteamSession()
		? TEXT("Steam 오버레이에서 친구를 방으로 초대합니다. 방이 가득 찼거나 게임 시작 중에는 초대할 수 없습니다.")
		: TEXT("Steam 로그인 상태로 실행해야 친구를 초대할 수 있습니다.")));
}

void UASLobbyWidget::InviteFriends()
{
	UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>();
	if ((!Sessions || !Sessions->ShowInviteFriendsUI()) && LobbyStatusText)
	{
		LobbyStatusText->SetText(FText::FromString(TEXT("초대 창을 열지 못했습니다. Steam 로그인 및 게임 내 오버레이 설정을 확인해 주세요.")));
	}
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
	LeaveConfirmation = CreateWidget<UASLeaveRoomConfirmationWidget>(GetOwningPlayer());
	if (!LeaveConfirmation) return;
	LeaveConfirmation->SetReturnFocusWidget(this);
	LeaveConfirmation->OnConfirmed.BindUObject(this, &ThisClass::ConfirmLeaveRoom);
	LeaveConfirmation->AddToViewport(200);
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
