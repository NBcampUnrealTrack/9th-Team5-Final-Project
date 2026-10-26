#include "Characters/Player/UI/ASCharacterSelectWidget.h"

#include "Characters/Player/Data/ASCharacterRoster.h"
#include "Core/Framework/ASCharacterSelectSubsystem.h"
#include "Engine/GameInstance.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "Core/Framework/ASLobbyPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

void UASCharacterSelectWidget::NativeConstruct()
{
	const AASLobbyGameState* LobbyState = GetWorld() ? GetWorld()->GetGameState<AASLobbyGameState>() : nullptr;
	if (LobbyState && LobbyState->CharacterRoster) Roster = LobbyState->CharacterRoster;
	Super::NativeConstruct();
	
	// ESC 를 받으려면 포커스가 있어야 한다
	SetIsFocusable(true);
	SetKeyboardFocus();
	
	if (!Roster || GetCharacterCount() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] %s 에 Roster 가 비어 있다."), *GetName());
		return;
	}
	
	// 열릴 때 이전 선택을 미리 골라 둔다 (없으면 0번)
	SelectCharacter(FindPreviousSelectionIndex());
	if (GetOwningPlayer<AASLobbyPlayerController>())
	{
		if (CharacterReadyButton) CharacterReadyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestConfirm);
		GetWorld()->GetTimerManager().SetTimer(SelectionDisplayTimer, this, &ThisClass::UpdateNetworkSelectionDisplay, 0.2f, true);
		UpdateNetworkSelectionDisplay();
	}
}

void UASCharacterSelectWidget::NativeDestruct()
{
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
	if (LobbyController && (IsCharacterConfirmed() || GetSelectionTimeRemaining() <= 0.0f)) return;
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
		if (GetSelectionTimeRemaining() > 0.0f && !IsCharacterConfirmed())
		{
			LobbyController->SelectLobbyCharacter(SelectedIndex);
			LobbyController->SetCharacterReady(true);
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
	if (GetOwningPlayer<AASLobbyPlayerController>()) return;
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
	if (CharacterReadyButton) CharacterReadyButton->SetIsEnabled(!bConfirmed && GetSelectionTimeRemaining() > 0.0f);
	if (CharacterSelectionStatusText)
	{
		CharacterSelectionStatusText->SetText(FText::FromString(bConfirmed
			? TEXT("준비 완료. 다른 플레이어를 기다리고 있습니다.")
			: TEXT("캐릭터를 선택하고 준비를 눌러 주세요.")));
	}
}
