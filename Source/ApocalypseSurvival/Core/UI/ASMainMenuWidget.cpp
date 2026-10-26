#include "Core/UI/ASMainMenuWidget.h"

#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/TextBlock.h"

void UASMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UGameInstance* GameInstance = GetGameInstance();
	SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<UASSessionSubsystem>() : nullptr;
	if (SessionSubsystem)
	{
		SessionSubsystem->OnSessionCreated.AddUniqueDynamic(this, &ThisClass::HandleSessionCreated);
		if (ConnectionStatusText)
		{
			ConnectionStatusText->SetText(SessionSubsystem->ReturnToMenuMessage);
		}
	}
}

void UASMainMenuWidget::NativeDestruct()
{
	if (SessionSubsystem)
	{
		SessionSubsystem->OnSessionCreated.RemoveDynamic(this, &ThisClass::HandleSessionCreated);
	}

	Super::NativeDestruct();
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
	UE_LOG(LogTemp, Warning, TEXT("[Session] %s"), *Message.ToString());
	OnHostRoomFailed(Message);
}

