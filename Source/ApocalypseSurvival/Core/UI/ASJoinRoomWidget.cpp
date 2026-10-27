#include "Core/UI/ASJoinRoomWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Components/EditableTextBox.h"

void UASJoinRoomWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UGameInstance* Instance = GetGameInstance();
	SessionSubsystem = Instance ? Instance->GetSubsystem<UASSessionSubsystem>() : nullptr;
	if (SessionSubsystem)
	{
		SessionSubsystem->OnSessionsFound.AddUniqueDynamic(this, &ThisClass::HandleSessionsFound);
		SessionSubsystem->OnSessionJoined.AddUniqueDynamic(this, &ThisClass::HandleSessionJoined);
	}
	if (SearchRoomsButton) SearchRoomsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SearchRooms);
	if (JoinRoomButton) JoinRoomButton->OnClicked.AddUniqueDynamic(this, &ThisClass::JoinSelectedRoom);
	if (RoomComboBox) RoomComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleRoomSelectionChanged);
	if (PasswordTextBox)
	{
		PasswordTextBox->SetIsPassword(true);
		PasswordTextBox->SetHintText(FText::FromString(TEXT("비밀번호 방: 숫자 4자리")));
		PasswordTextBox->SetText(FText::GetEmpty());
		LastValidPassword.Reset();
		PasswordTextBox->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandlePasswordChanged);
	}
	UpdatePasswordVisibility();
	UpdateButtons();
}

void UASJoinRoomWidget::NativeDestruct()
{
	if (SessionSubsystem)
	{
		SessionSubsystem->OnSessionsFound.RemoveDynamic(this, &ThisClass::HandleSessionsFound);
		SessionSubsystem->OnSessionJoined.RemoveDynamic(this, &ThisClass::HandleSessionJoined);
	}
	if (SearchRoomsButton) SearchRoomsButton->OnClicked.RemoveDynamic(this, &ThisClass::SearchRooms);
	if (JoinRoomButton) JoinRoomButton->OnClicked.RemoveDynamic(this, &ThisClass::JoinSelectedRoom);
	if (RoomComboBox) RoomComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleRoomSelectionChanged);
	if (PasswordTextBox) PasswordTextBox->OnTextChanged.RemoveDynamic(this, &ThisClass::HandlePasswordChanged);
	Super::NativeDestruct();
}

void UASJoinRoomWidget::SearchRooms()
{
	if (bSearching || bJoining) return;
	if (!SessionSubsystem)
	{
		SetStatus(TEXT("세션 기능을 사용할 수 없습니다."));
		return;
	}
	AvailableRooms.Reset();
	if (RoomComboBox) RoomComboBox->ClearOptions();
	UpdatePasswordVisibility();
	bSearching = true;
	UpdateButtons();
	SetStatus(TEXT("방 검색 중..."));
	SessionSubsystem->FindSessions();
}

void UASJoinRoomWidget::HandleSessionsFound(const bool bSuccess, const TArray<FASSessionSearchResult>& Results)
{
	if (!bSearching) return;
	bSearching = false;
	AvailableRooms = bSuccess ? Results : TArray<FASSessionSearchResult>();
	if (RoomComboBox)
	{
		RoomComboBox->ClearOptions();
		for (const FASSessionSearchResult& Room : AvailableRooms)
		{
			RoomComboBox->AddOption(FString::Printf(TEXT("[%d] %s%s (%d/%d)"), Room.Index,
				Room.bRequiresPassword ? TEXT("[비밀번호] ") : TEXT(""), *Room.RoomName, Room.CurrentPlayers, Room.MaxPlayers));
		}
		if (!AvailableRooms.IsEmpty()) RoomComboBox->SetSelectedIndex(0);
	}
	UpdatePasswordVisibility();
	UpdateButtons();
	SetStatus(bSuccess ? FString::Printf(TEXT("검색된 방: %d개"), AvailableRooms.Num()) : TEXT("방 검색에 실패했습니다."));
	OnRoomListUpdated(bSuccess);
}

void UASJoinRoomWidget::HandleRoomSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	LastValidPassword.Reset();
	if (PasswordTextBox) PasswordTextBox->SetText(FText::GetEmpty());
	UpdatePasswordVisibility();
}

void UASJoinRoomWidget::UpdatePasswordVisibility()
{
	if (!PasswordTextBox) return;
	const int32 Selection = RoomComboBox ? RoomComboBox->GetSelectedIndex() : INDEX_NONE;
	const bool bRequiresPassword = AvailableRooms.IsValidIndex(Selection) && AvailableRooms[Selection].bRequiresPassword;
	PasswordTextBox->SetVisibility(bRequiresPassword ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UASJoinRoomWidget::JoinSelectedRoom()
{
	const int32 Selection = RoomComboBox ? RoomComboBox->GetSelectedIndex() : INDEX_NONE;
	if (AvailableRooms.IsValidIndex(Selection)) JoinRoom(AvailableRooms[Selection].Index);
}

void UASJoinRoomWidget::JoinRoom(const int32 ResultIndex)
{
	if (bSearching || bJoining || !SessionSubsystem) return;
	const FASSessionSearchResult* Room = AvailableRooms.FindByPredicate([ResultIndex](const FASSessionSearchResult& Entry) { return Entry.Index == ResultIndex; });
	if (!Room) return;
	const FString Password = PasswordTextBox ? PasswordTextBox->GetText().ToString() : FString();
	if (Room->bRequiresPassword && !UASSessionSubsystem::IsValidRoomPassword(Password))
	{
		SetStatus(TEXT("비밀번호를 숫자 4자리로 입력한 후 참가해 주세요."));
		if (PasswordTextBox) PasswordTextBox->SetKeyboardFocus();
		return;
	}
	bJoining = true;
	UpdateButtons();
	SetStatus(TEXT("방 참가 중..."));
	SessionSubsystem->JoinSessionByIndex(ResultIndex, Password);
}

void UASJoinRoomWidget::HandlePasswordChanged(const FText& Text)
{
	const FString Input = Text.ToString();
	bool bValid = Input.Len() <= 4;
	for (const TCHAR Character : Input)
	{
		bValid &= Character >= TEXT('0') && Character <= TEXT('9');
	}
	if (bValid) LastValidPassword = Input;
	else PasswordTextBox->SetText(FText::FromString(LastValidPassword));
}

void UASJoinRoomWidget::HandleSessionJoined(const bool bSuccess, const FString& ErrorMessage)
{
	if (!bJoining) return;
	if (!bSuccess) bJoining = false;
	UpdateButtons();
	SetStatus(bSuccess ? TEXT("서버에 연결 중...") : ErrorMessage);
}

void UASJoinRoomWidget::SetStatus(const FString& Message)
{
	if (RoomStatusText) RoomStatusText->SetText(FText::FromString(Message));
	UE_LOG(LogTemp, Log, TEXT("[Session] %s"), *Message);
}

void UASJoinRoomWidget::UpdateButtons()
{
	if (SearchRoomsButton) SearchRoomsButton->SetIsEnabled(!bSearching && !bJoining);
	if (JoinRoomButton) JoinRoomButton->SetIsEnabled(!bSearching && !bJoining && !AvailableRooms.IsEmpty());
}
