#include "Characters/Player/UI/ASCharacterSelectWidget.h"

#include "Characters/Player/Data/ASCharacterRoster.h"
#include "Core/Framework/ASCharacterSelectSubsystem.h"
#include "Engine/GameInstance.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "Core/Framework/ASLobbyPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Core/UI/ASLeaveRoomConfirmationWidget.h"
#include "Core/UI/ASUIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Core/Framework/ASSessionSubsystem.h"

void UASCharacterSelectWidget::NativeConstruct()
{
	const AASLobbyGameState* LobbyState = GetWorld() ? GetWorld()->GetGameState<AASLobbyGameState>() : nullptr;
	if (LobbyState && LobbyState->CharacterRoster) Roster = LobbyState->CharacterRoster;
	Super::NativeConstruct();
	
	// ESC 를 받으려면 포커스가 있어야 한다
	SetIsFocusable(true);
	SetKeyboardFocus();
	if (GetOwningPlayer<AASLobbyPlayerController>())
	{
		CreateLeaveRoomButton();
		if (LeaveRoomButton) LeaveRoomButton->OnClicked.AddUniqueDynamic(this, &ThisClass::LeaveRoom);
	}
	
	if (!Roster || GetCharacterCount() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] %s 에 Roster 가 비어 있다."), *GetName());
		return;
	}
	
	// 열릴 때 이전 선택을 미리 골라 둔다 (없으면 0번)
	SelectCharacter(FindPreviousSelectionIndex());
	if (GetOwningPlayer<AASLobbyPlayerController>())
	{
		bReadyRequestPending = false;
		CreatePlayerListDisplay();
		if (CharacterReadyButton) CharacterReadyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestConfirm);
		GetWorld()->GetTimerManager().SetTimer(SelectionDisplayTimer, this, &ThisClass::UpdateNetworkSelectionDisplay, 0.2f, true);
		UpdateNetworkSelectionDisplay();
	}
}

void UASCharacterSelectWidget::NativeDestruct()
{
	if (LeaveConfirmation) LeaveConfirmation->RemoveFromParent();
	if (LeaveRoomButton) LeaveRoomButton->OnClicked.RemoveDynamic(this, &ThisClass::LeaveRoom);
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SelectionDisplayTimer);
	if (CharacterReadyButton) CharacterReadyButton->OnClicked.RemoveDynamic(this, &ThisClass::RequestConfirm);
	Super::NativeDestruct();
}

FReply UASCharacterSelectWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == BackKey)
	{
		RequestBack();
		return FReply::Handled();
	}
	
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UASCharacterSelectWidget::GetCharacterCount() const
{
	return Roster ? Roster->Characters.Num() : 0;
}

bool UASCharacterSelectWidget::GetCharacterInfo(int32 Index, FASCharacterDisplayInfo& OutInfo) const
{
	const UASCharacterDefinition* Definition = GetDefinitionAt(Index);
	if (!Definition)
	{
		return false;
	}
	
	OutInfo.DisplayName = Definition->DisplayName;
	OutInfo.Title = Definition->Title;
	OutInfo.AbilityName = Definition->AbilityName;
	OutInfo.Role = Definition->Role;
	OutInfo.CombatStyle = Definition->CombatStyle;
	OutInfo.DifficultyText = UEnum::GetDisplayValueAsText(Definition->Difficulty);
	OutInfo.RepresentativeWeapons = Definition->RepresentativeWeapons;
	OutInfo.Description = Definition->Description;
	OutInfo.Icon = Definition->Icon;
	OutInfo.SkillDisplays = Definition->SkillDisplays;
	return true;
}

void UASCharacterSelectWidget::SelectCharacter(int32 Index)
{
	AASLobbyPlayerController* LobbyController = GetOwningPlayer<AASLobbyPlayerController>();
	if (LobbyController && (bReadyRequestPending || IsCharacterConfirmed() || GetSelectionTimeRemaining() <= 0.0f)) return;
	FASCharacterDisplayInfo Info;
	if (!GetCharacterInfo(Index, Info))
	{
		return;
	}
	
	SelectedIndex = Index;
	OnSelectionChanged(SelectedIndex, Info);
	if (LobbyController)
	{
		LobbyController->SelectLobbyCharacter(Index);
	}
}

void UASCharacterSelectWidget::RequestConfirm()
{
	if (AASLobbyPlayerController* LobbyController = GetOwningPlayer<AASLobbyPlayerController>())
	{
		if (GetSelectionTimeRemaining() > 0.0f && !bReadyRequestPending)
		{
			bRequestedReady = !IsCharacterConfirmed();
			bReadyRequestPending = true;
			if (bRequestedReady) LobbyController->SelectLobbyCharacter(SelectedIndex);
			LobbyController->SetCharacterReady(bRequestedReady);
			UpdateNetworkSelectionDisplay();
		}
		return;
	}
	if (ConfirmSelection())
	{
		OnConfirmed.Broadcast();
	}
}

void UASCharacterSelectWidget::RequestBack()
{
	if (GetOwningPlayer<AASLobbyPlayerController>())
	{
		LeaveRoom();
		return;
	}
	OnBackRequested.Broadcast();
}

bool UASCharacterSelectWidget::ConfirmSelection()
{
	const UASCharacterDefinition* Definition = GetDefinitionAt(SelectedIndex);
	UGameInstance* GameInstance = GetGameInstance();
	UASCharacterSelectSubsystem* SelectSubsystem = GameInstance ? GameInstance->GetSubsystem<UASCharacterSelectSubsystem>() : nullptr;
	if (!Definition || !SelectSubsystem)
	{
		return false;
	}
	
	SelectSubsystem->SetSelectedCharacter(Definition);
	return true;
}

const UASCharacterDefinition* UASCharacterSelectWidget::GetDefinitionAt(int32 Index) const
{
	return Roster ? Roster->GetCharacterAt(Index) : nullptr;
}

int32 UASCharacterSelectWidget::FindPreviousSelectionIndex() const
{
	if (const AASLobbyPlayerController* Controller = GetOwningPlayer<AASLobbyPlayerController>())
	{
		const AASLobbyGameState* State = GetWorld()->GetGameState<AASLobbyGameState>();
		if (State && Controller->PlayerState)
		{
			const int32 LocalId = Controller->PlayerState->GetPlayerId();
			const FASLobbyPlayerInfo* Info = State->LobbyPlayers.FindByPredicate([LocalId](const FASLobbyPlayerInfo& Player)
				{ return Player.PlayerId == LocalId; });
			if (Info && GetDefinitionAt(Info->CharacterIndex)) return Info->CharacterIndex;
		}
		return 0;
	}
	const UGameInstance* GameInstance = GetGameInstance();
	const UASCharacterSelectSubsystem* SelectSubsystem = GameInstance ? GameInstance->GetSubsystem<UASCharacterSelectSubsystem>() : nullptr;
	const UASCharacterDefinition* Previous = SelectSubsystem ? SelectSubsystem->GetSelectedCharacter() : nullptr;
	if (!Previous || !Roster)
	{
		return 0;
	}
	
	const int32 Found = Roster->Characters.IndexOfByKey(Previous);
	return Found != INDEX_NONE ? Found : 0;
}

float UASCharacterSelectWidget::GetSelectionTimeRemaining() const
{
	const AASLobbyGameState* State = GetWorld() ? GetWorld()->GetGameState<AASLobbyGameState>() : nullptr;
	return State ? State->GetCharacterSelectionTimeRemaining() : 0.0f;
}

bool UASCharacterSelectWidget::IsCharacterConfirmed() const
{
	const APlayerController* Controller = GetOwningPlayer();
	const AASLobbyGameState* State = GetWorld() ? GetWorld()->GetGameState<AASLobbyGameState>() : nullptr;
	if (!Controller || !Controller->PlayerState || !State) return false;
	const int32 LocalId = Controller->PlayerState->GetPlayerId();
	const FASLobbyPlayerInfo* Info = State->LobbyPlayers.FindByPredicate([LocalId](const FASLobbyPlayerInfo& Player)
		{ return Player.PlayerId == LocalId; });
	return Info && Info->bCharacterReady;
}

void UASCharacterSelectWidget::UpdateNetworkSelectionDisplay()
{
	if (CharacterSelectionTimeText)
	{
		CharacterSelectionTimeText->SetText(FText::AsNumber(FMath::CeilToInt(GetSelectionTimeRemaining())));
	}
	const bool bConfirmed = IsCharacterConfirmed();
	if (bReadyRequestPending && bConfirmed == bRequestedReady) bReadyRequestPending = false;
	const bool bCanToggleReady = !bReadyRequestPending && GetSelectionTimeRemaining() > 0.0f;
	if (CharacterReadyButton) CharacterReadyButton->SetIsEnabled(bCanToggleReady);
	if (ConfirmButton) ConfirmButton->SetIsEnabled(bCanToggleReady);
	if (ConfirmText) ConfirmText->SetText(FText::FromString(bReadyRequestPending ? TEXT("처리 중...")
		: bConfirmed ? TEXT("확정 취소") : TEXT("캐릭터 확정")));
	if (CharacterSelectionStatusText)
	{
		CharacterSelectionStatusText->SetText(FText::FromString(bConfirmed
			? TEXT("확정 완료. 게임 이동 전까지 확정을 취소할 수 있습니다.")
			: TEXT("캐릭터를 선택하고 준비를 눌러 주세요.")));
	}
	if (CharacterPlayerListText)
	{
		FString List = TEXT("플레이어 선택 현황\n");
		const APlayerController* Controller = GetOwningPlayer();
		const AASLobbyGameState* State = GetWorld() ? GetWorld()->GetGameState<AASLobbyGameState>() : nullptr;
		if (State)
		{
			for (const FASLobbyPlayerInfo& Player : State->LobbyPlayers)
			{
				const UASCharacterDefinition* Definition = State->CharacterRoster
					? State->CharacterRoster->GetCharacterAt(Player.CharacterIndex) : nullptr;
				const bool bLocal = Controller && Controller->PlayerState && Controller->PlayerState->GetPlayerId() == Player.PlayerId;
				List += FString::Printf(TEXT("%s%s : %s · %s\n"), *Player.PlayerName, bLocal ? TEXT(" (나)") : TEXT(""),
					Definition ? *Definition->DisplayName.ToString() : TEXT("선택 안 함"),
					Player.bCharacterReady ? TEXT("확정 완료") : TEXT("선택 중"));
			}
		}
		CharacterPlayerListText->SetText(FText::FromString(List));
	}
}

void UASCharacterSelectWidget::CreatePlayerListDisplay()
{
	if (CharacterPlayerListText) return;
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!Canvas) return;
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.03f, 0.9f));
	Background->SetPadding(FMargin(12.0f));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(400.0f);
	Background->SetContent(Size);
	CharacterPlayerListText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CharacterPlayerListText"));
	CharacterPlayerListText->SetAutoWrapText(true);
	FSlateFontInfo Font = CharacterPlayerListText->GetFont();
	Font.Size = 18;
	CharacterPlayerListText->SetFont(Font);
	Size->SetContent(CharacterPlayerListText);
	UCanvasPanelSlot* ListSlot = Canvas->AddChildToCanvas(Background);
	ListSlot->SetPosition(FVector2D(680.0f, 132.0f));
	ListSlot->SetAutoSize(true);
	ListSlot->SetZOrder(10);
}

void UASCharacterSelectWidget::CreateLeaveRoomButton()
{
	if (LeaveRoomButton) return;
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!Canvas) return;
	LeaveRoomButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LeaveRoomButton"));
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(FText::FromString(TEXT("방 나가기")));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	LeaveRoomButton->SetContent(Label);
	UCanvasPanelSlot* ButtonSlot = Canvas->AddChildToCanvas(LeaveRoomButton);
	ButtonSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	ButtonSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	ButtonSlot->SetPosition(FVector2D(0.0f, -24.0f));
	ButtonSlot->SetSize(FVector2D(200.0f, 48.0f));
	ButtonSlot->SetZOrder(10);
}

void UASCharacterSelectWidget::LeaveRoom()
{
	const AASLobbyGameState* State = GetWorld() ? GetWorld()->GetGameState<AASLobbyGameState>() : nullptr;
	if (State && State->LobbyPhase == EASLobbyPhase::Travelling) return;
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

void UASCharacterSelectWidget::ConfirmLeaveRoom()
{
	if (UASSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UASSessionSubsystem>())
	{
		if (LeaveRoomButton) LeaveRoomButton->SetIsEnabled(false);
		Sessions->LeaveRoom();
	}
}
