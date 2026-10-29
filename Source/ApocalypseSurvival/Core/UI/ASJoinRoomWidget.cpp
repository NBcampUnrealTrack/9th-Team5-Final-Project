#include "Core/UI/ASJoinRoomWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Components/EditableTextBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UASJoinRoomWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureRoomCodeControls();
	UGameInstance* Instance = GetGameInstance();
	SessionSubsystem = Instance ? Instance->GetSubsystem<UASSessionSubsystem>() : nullptr;
	if (SessionSubsystem)
	{
		SessionSubsystem->OnSessionsFound.AddUniqueDynamic(this, &ThisClass::HandleSessionsFound);
		SessionSubsystem->OnSessionJoined.AddUniqueDynamic(this, &ThisClass::HandleSessionJoined);
	}
	if (SearchRoomsButton) SearchRoomsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SearchRooms);
	if (JoinRoomButton) JoinRoomButton->OnClicked.AddUniqueDynamic(this, &ThisClass::JoinSelectedRoom);
	if (JoinByCodeButton) JoinByCodeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::JoinRoomByCode);
	if (RoomComboBox) RoomComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleRoomSelectionChanged);
	if (PasswordTextBox)
	{
		PasswordTextBox->SetIsPassword(true);
		PasswordTextBox->SetHintText(FText::FromString(TEXT("비밀번호 방: 숫자 4자리")));
		PasswordTextBox->SetText(FText::GetEmpty());
		LastValidPassword.Reset();
		PasswordTextBox->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandlePasswordChanged);
	}
	if (RoomCodeTextBox)
	{
		RoomCodeTextBox->SetHintText(FText::FromString(TEXT("방 코드 6자리")));
		LastValidRoomCode = RoomCodeTextBox->GetText().ToString();
		RoomCodeTextBox->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleRoomCodeChanged);
	}
	UpdatePasswordVisibility();
	UpdateButtons();
}

void UASJoinRoomWidget::EnsureRoomCodeControls()
{
	if (RoomCodeTextBox && JoinByCodeButton) return;
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!Canvas) return;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.04f, 0.06f, 0.10f, 0.95f));
	Panel->SetPadding(FMargin(20));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(1, 0.5f));
	PanelSlot->SetAlignment(FVector2D(1, 0.5f));
	PanelSlot->SetPosition(FVector2D(-24, 0));
	PanelSlot->SetSize(FVector2D(340, 300));
	PanelSlot->SetZOrder(10);
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Layout);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(FText::FromString(TEXT("방 코드로 참가")));
	Layout->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 12));
	if (!RoomCodeTextBox)
	{
		RoomCodeTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("RoomCodeTextBox"));
		Layout->AddChildToVerticalBox(RoomCodeTextBox)->SetPadding(FMargin(0, 0, 0, 12));
	}
	CodePasswordTextBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	CodePasswordTextBox->SetIsPassword(true);
	CodePasswordTextBox->SetHintText(FText::FromString(TEXT("비밀번호 방만 입력 (숫자 4자리)")));
	Layout->AddChildToVerticalBox(CodePasswordTextBox)->SetPadding(FMargin(0, 0, 0, 12));
	if (!JoinByCodeButton)
	{
		JoinByCodeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("JoinByCodeButton"));
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(TEXT("코드로 참가")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Label->SetMargin(FMargin(8));
		JoinByCodeButton->SetContent(Label);
		Layout->AddChildToVerticalBox(JoinByCodeButton)->SetPadding(FMargin(0, 0, 0, 12));
	}
	CodeStatusText = WidgetTree->ConstructWidget<UTextBlock>();
	CodeStatusText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(CodeStatusText);
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
	if (JoinByCodeButton) JoinByCodeButton->OnClicked.RemoveDynamic(this, &ThisClass::JoinRoomByCode);
	if (RoomComboBox) RoomComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleRoomSelectionChanged);
	if (PasswordTextBox) PasswordTextBox->OnTextChanged.RemoveDynamic(this, &ThisClass::HandlePasswordChanged);
	if (RoomCodeTextBox) RoomCodeTextBox->OnTextChanged.RemoveDynamic(this, &ThisClass::HandleRoomCodeChanged);
	Super::NativeDestruct();
}

void UASJoinRoomWidget::JoinRoomByCode()
{
	if (bSearching || bJoining || !SessionSubsystem || !RoomCodeTextBox) return;
	const UEditableTextBox* PasswordInput = CodePasswordTextBox ? CodePasswordTextBox.Get() : PasswordTextBox.Get();
	const FString Password = PasswordInput ? PasswordInput->GetText().ToString() : FString();
	// A code search replaces the subsystem's indexed search results.
	AvailableRooms.Reset();
	if (RoomComboBox) RoomComboBox->ClearOptions();
	bJoining = true;
	UpdateButtons();
	SetStatus(TEXT("방 코드를 확인하는 중..."));
	SessionSubsystem->JoinSessionByRoomCode(RoomCodeTextBox->GetText().ToString(), Password);
}

void UASJoinRoomWidget::HandleRoomCodeChanged(const FText& Text)
{
	const FString Input = Text.ToString();
	bool bValid = Input.Len() <= 6;
	for (const TCHAR Character : Input) bValid &= Character >= TEXT('0') && Character <= TEXT('9');
	if (bValid) LastValidRoomCode = Input;
	else RoomCodeTextBox->SetText(FText::FromString(LastValidRoomCode));
	UpdatePasswordVisibility();
	UpdateButtons();
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
	SetStatus(bSuccess ? FString::Printf(TEXT("검색된 방: %d개"), AvailableRooms.Num())
		: SessionSubsystem && !SessionSubsystem->IsSessionServiceReady() ? SessionSubsystem->GetConnectionStatusText().ToString()
		: TEXT("방 검색에 실패했습니다."));
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
	const bool bCodeEntry = !CodePasswordTextBox && RoomCodeTextBox && !RoomCodeTextBox->GetText().IsEmpty();
	PasswordTextBox->SetVisibility(bRequiresPassword || bCodeEntry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
	if (CodeStatusText) CodeStatusText->SetText(FText::FromString(Message));
	UE_LOG(LogTemp, Log, TEXT("[Session] %s"), *Message);
}

void UASJoinRoomWidget::UpdateButtons()
{
	if (SearchRoomsButton) SearchRoomsButton->SetIsEnabled(!bSearching && !bJoining);
	if (JoinRoomButton) JoinRoomButton->SetIsEnabled(!bSearching && !bJoining && !AvailableRooms.IsEmpty());
	if (JoinByCodeButton) JoinByCodeButton->SetIsEnabled(!bSearching && !bJoining && RoomCodeTextBox
		&& UASSessionSubsystem::IsValidRoomCode(RoomCodeTextBox->GetText().ToString()));
	if (RoomCodeTextBox) RoomCodeTextBox->SetIsEnabled(!bSearching && !bJoining);
	if (CodePasswordTextBox) CodePasswordTextBox->SetIsEnabled(!bSearching && !bJoining);
}
