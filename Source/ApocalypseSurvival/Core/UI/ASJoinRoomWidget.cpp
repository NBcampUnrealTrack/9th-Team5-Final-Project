#include "Core/UI/ASJoinRoomWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

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
			RoomComboBox->AddOption(FString::Printf(TEXT("[%d] %s (%d/%d)"), Room.Index, *Room.RoomName, Room.CurrentPlayers, Room.MaxPlayers));
		}
		if (!AvailableRooms.IsEmpty()) RoomComboBox->SetSelectedIndex(0);
	}
	UpdateButtons();
	SetStatus(bSuccess ? FString::Printf(TEXT("검색된 방: %d개"), AvailableRooms.Num()) : TEXT("방 검색에 실패했습니다."));
	OnRoomListUpdated(bSuccess);
}

void UASJoinRoomWidget::JoinSelectedRoom()
{
	const int32 Selection = RoomComboBox ? RoomComboBox->GetSelectedIndex() : INDEX_NONE;
	if (AvailableRooms.IsValidIndex(Selection)) JoinRoom(AvailableRooms[Selection].Index);
}

void UASJoinRoomWidget::JoinRoom(const int32 ResultIndex)
{
	if (bSearching || bJoining || !SessionSubsystem) return;
	if (!AvailableRooms.ContainsByPredicate([ResultIndex](const FASSessionSearchResult& Room) { return Room.Index == ResultIndex; })) return;
	bJoining = true;
	UpdateButtons();
	SetStatus(TEXT("방 참가 중..."));
	SessionSubsystem->JoinSessionByIndex(ResultIndex);
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
