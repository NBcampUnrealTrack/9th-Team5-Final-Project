#include "Core/UI/ASMainMenuWidget.h"

#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Core/UI/ASJoinRoomWidget.h"

void UASMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ConnectionStatusText)
	{
		if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(GetRootWidget()))
		{
			ConnectionStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConnectionStatusText"));
			ConnectionStatusText->SetAutoWrapText(true);
			ConnectionStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.4f)));
			ConnectionStatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* StatusSlot = Canvas->AddChildToCanvas(ConnectionStatusText);
			StatusSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
			StatusSlot->SetOffsets(FMargin(24.0f, 20.0f, 24.0f, 80.0f));
			StatusSlot->SetZOrder(10);
		}
	}
	if (ScreenSwitcher)
	{
		ScreenSwitcher->RemoveAllFieldValueChangedDelegates(this);
		ScreenSwitcher->AddFieldValueChangedDelegate(UWidgetSwitcher::FFieldNotificationClassDescriptor::ActiveWidgetIndex,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this,
				[this](UObject*, UE::FieldNotification::FFieldId) { SearchActiveJoinRoom(); }));
	}

	if (ScreenSwitcher)
	{
		for (int32 Index = 0; Index < ScreenSwitcher->GetChildrenCount(); ++Index)
		{
			UUserWidget* Panel = Cast<UUserWidget>(ScreenSwitcher->GetChildAt(Index));
			UButton* Button = Panel ? Cast<UButton>(Panel->GetWidgetFromName(TEXT("CreateRoomButton"))) : nullptr;
			if (!Button) continue;
			CreateRoomPanel = Panel;
			CreateRoomValidationButton = Button;
			Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ValidateRoomInput);
			break;
		}
	}
	UGameInstance* GameInstance = GetGameInstance();
	SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<UASSessionSubsystem>() : nullptr;
	if (SessionSubsystem)
	{
		SessionSubsystem->OnSessionCreated.AddUniqueDynamic(this, &ThisClass::HandleSessionCreated);
		if (ConnectionStatusText)
		{
			ConnectionStatusText->SetText(SessionSubsystem->ReturnToMenuMessage);
			SessionSubsystem->ReturnToMenuMessage = FText::GetEmpty();
		}
	}
	SearchActiveJoinRoom();
}

void UASMainMenuWidget::NativeDestruct()
{
	if (CreateRoomValidationButton) CreateRoomValidationButton->OnClicked.RemoveDynamic(this, &ThisClass::ValidateRoomInput);
	if (ScreenSwitcher) ScreenSwitcher->RemoveAllFieldValueChangedDelegates(this);
	if (SessionSubsystem)
	{
		SessionSubsystem->OnSessionCreated.RemoveDynamic(this, &ThisClass::HandleSessionCreated);
	}

	Super::NativeDestruct();
}

void UASMainMenuWidget::SearchActiveJoinRoom()
{
	UASJoinRoomWidget* JoinPanel = ScreenSwitcher ? Cast<UASJoinRoomWidget>(ScreenSwitcher->GetActiveWidget()) : nullptr;
	if (JoinPanel) JoinPanel->SearchRooms();
}

void UASMainMenuWidget::HostRoom(const FString& RoomName, const bool bUsePassword, const FString& Password)
{
	if (bIsHosting)
	{
		return;
	}

	const FString TrimmedRoomName = RoomName.TrimStartAndEnd();
	if (TrimmedRoomName.IsEmpty())
	{
		FailHosting(FText::FromString(TEXT("방 제목을 입력해 주세요.")));
		return;
	}

	if (bUsePassword)
	{
		bool bValidPassword = Password.Len() == 4;
		for (const TCHAR Character : Password)
		{
			bValidPassword &= Character >= TEXT('0') && Character <= TEXT('9');
		}
		if (!bValidPassword)
		{
			FailHosting(FText::FromString(TEXT("비밀번호는 숫자 4자리로 입력해 주세요.")));
			return;
		}
	}

	if (!SessionSubsystem || LobbyMapPath.IsEmpty())
	{
		FailHosting(FText::FromString(TEXT("세션 또는 로비 맵 설정을 확인해 주세요.")));
		return;
	}

	if (ConnectionStatusText) ConnectionStatusText->SetText(FText::GetEmpty());
	bIsHosting = true;
	SessionSubsystem->CreateSession(PublicConnections, TrimmedRoomName, bUsePassword, Password);
}

void UASMainMenuWidget::HandleSessionCreated(const bool bSuccess)
{
	if (!bIsHosting)
	{
		return;
	}

	if (!bSuccess)
	{
		FailHosting(FText::FromString(TEXT("방 생성에 실패했습니다.")));
		return;
	}

	if (!SessionSubsystem->HostTravelToMap(LobbyMapPath))
	{
		SessionSubsystem->DestroySession();
		FailHosting(FText::FromString(TEXT("로비 맵 이동 요청에 실패했습니다.")));
	}
}

void UASMainMenuWidget::FailHosting(const FText& Message)
{
	bIsHosting = false;
	if (ConnectionStatusText) ConnectionStatusText->SetText(Message);
	UE_LOG(LogTemp, Warning, TEXT("[Session] %s"), *Message.ToString());
	OnHostRoomFailed(Message);
}


void UASMainMenuWidget::ValidateRoomInput()
{
	UUserWidget* Panel = CreateRoomPanel.Get();
	if (!Panel || bIsHosting) return;
	const UEditableTextBox* Name = Cast<UEditableTextBox>(Panel->GetWidgetFromName(TEXT("RoomNameTextBox")));
	const UCheckBox* UsePassword = Cast<UCheckBox>(Panel->GetWidgetFromName(TEXT("PasswordCheckBox")));
	const UEditableTextBox* Password = Cast<UEditableTextBox>(Panel->GetWidgetFromName(TEXT("PasswordTextBox")));
	if (Name && Name->GetText().ToString().TrimStartAndEnd().IsEmpty())
	{
		FailHosting(FText::FromString(TEXT("방 제목을 입력해 주세요.")));
	}
	else if (UsePassword && UsePassword->IsChecked() && Password)
	{
		const FString Input = Password->GetText().ToString();
		bool bValid = Input.Len() == 4;
		for (const TCHAR Character : Input) bValid &= Character >= TEXT('0') && Character <= TEXT('9');
		if (!bValid) FailHosting(FText::FromString(TEXT("비밀번호는 숫자 4자리로 입력해 주세요.")));
	}
}
