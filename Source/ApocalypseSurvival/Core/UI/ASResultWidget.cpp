#include "Core/UI/ASResultWidget.h"

#include "Components/Button.h"
#include "Core/Framework/ASGameMode.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UASResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (MainMenuButton)
	{
		// The result screen's old Blueprint handler opens the map without destroying the session.
		MainMenuButton->OnClicked.Clear();
		MainMenuButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ReturnToLobby);
		const APlayerController* Player = GetOwningPlayer();
		const bool bHost = Player && Player->HasAuthority() && Player->IsLocalController();
		MainMenuButton->SetIsEnabled(bHost);
		if (UTextBlock* Label = Cast<UTextBlock>(MainMenuButton->GetContent()))
		{
			Label->SetText(FText::FromString(bHost ? TEXT("로비로 돌아가기") : TEXT("방장의 로비 복귀를 기다리는 중")));
		}
	}
}

void UASResultWidget::NativeDestruct()
{
	if (MainMenuButton) MainMenuButton->OnClicked.RemoveDynamic(this, &ThisClass::ReturnToLobby);
	Super::NativeDestruct();
}

void UASResultWidget::ReturnToLobby()
{
	APlayerController* Player = GetOwningPlayer();
	AASGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AASGameMode>() : nullptr;
	if (!Mode || !Player || !Player->IsLocalController() || !Player->HasAuthority()) return;
	UGameplayStatics::SetGamePaused(this, false);
	if (Mode->ReturnToLobby() && MainMenuButton) MainMenuButton->SetIsEnabled(false);
}
