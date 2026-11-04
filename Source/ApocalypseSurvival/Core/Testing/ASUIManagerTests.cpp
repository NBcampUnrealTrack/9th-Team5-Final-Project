#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/UI/ASUIManagerSubsystem.h"
#include "Core/UI/ASInGameMenuWidget.h"
#include "Core/UI/ASLeaveRoomConfirmationWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Slate/SGameLayerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASUIManagerLifecycleTest,
	"ApocalypseSurvival.UI.Manager.ModalLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASUIManagerLifecycleTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(FName(TEXT("UIManagerTest")));
	UWorld* World = Instance->GetWorld();
	World->InitializeActorsForPlay(FURL());
	UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
	Instance->GetWorldContext()->GameViewport = Viewport;
	Viewport->Init(*Instance->GetWorldContext(), Instance, false);
	TSharedRef<SGameLayerManager> Layers = SNew(SGameLayerManager);
	Viewport->SetGameLayerManager(Layers);
	FString Error;
	ULocalPlayer* Local = Instance->CreateLocalPlayer(0, Error, false);
	APlayerController* Player = World->SpawnActor<APlayerController>();
	if (TestNotNull(TEXT("Local player"), Local) && TestNotNull(TEXT("Controller"), Player))
	{
		Player->SetPlayer(Local);
		Player->bShowMouseCursor = false;
		UASUIManagerSubsystem* UI = Local->GetSubsystem<UASUIManagerSubsystem>();
		if (TestNotNull(TEXT("Local UI manager"), UI))
		{
			UUserWidget* Menu = UI->OpenScreen(TEXT("Menu"), UASInGameMenuWidget::StaticClass(), 20);
			if (TestNotNull(TEXT("Menu opens"), Menu))
			{
				TestEqual(TEXT("Repeated open reuses the same screen"),
					UI->OpenScreen(TEXT("Menu"), UASInGameMenuWidget::StaticClass(), 20), Menu);
				TestTrue(TEXT("Menu is owned by this player"), Menu->GetOwningLocalPlayer() == Local);
				TestTrue(TEXT("Opening menu shows cursor"), Player->bShowMouseCursor);
				UUserWidget* Popup = UI->OpenScreen(TEXT("Popup"), UASLeaveRoomConfirmationWidget::StaticClass(), 200);
				if (TestNotNull(TEXT("Popup opens"), Popup))
				{
					Popup->RemoveFromParent();
					TestNull(TEXT("Self-closed popup is no longer open"), UI->GetScreen(TEXT("Popup")));
					TestEqual(TEXT("Underlying menu stays open"), UI->GetScreen(TEXT("Menu")), Menu);
					UI->OpenScreen(TEXT("Popup"), UASLeaveRoomConfirmationWidget::StaticClass(), 200);
				}
				UI->CloseScreen(TEXT("Menu"));
				TestNull(TEXT("Closing root closes menu"), UI->GetScreen(TEXT("Menu")));
				TestNull(TEXT("Closing root closes its popups"), UI->GetScreen(TEXT("Popup")));
				TestFalse(TEXT("Last close restores cursor state"), Player->bShowMouseCursor);
				UI->OpenScreen(TEXT("Menu"), UASInGameMenuWidget::StaticClass());
				UI->CloseAllScreens(false);
				TestTrue(TEXT("Travel/result cleanup leaves input owner untouched"), Player->bShowMouseCursor);
				UI->OpenScreen(TEXT("Menu"), UASInGameMenuWidget::StaticClass());
				UI->PlayerControllerChanged(nullptr);
				TestNull(TEXT("Controller change clears old UI"), UI->GetScreen(TEXT("Menu")));
				UUserWidget* MainMenu = UASUIManagerSubsystem::OpenScreenForPlayer(Player, TEXT("MainMenu"), UASInGameMenuWidget::StaticClass());
				TestNotNull(TEXT("HUD entry point opens a local managed menu"), MainMenu);
				UI->CloseScreen(TEXT("MainMenu"), false);
				UUserWidget* ExistingLobby = CreateWidget<UUserWidget>(Player, UASInGameMenuWidget::StaticClass());
				ExistingLobby->AddToPlayerScreen();
				TestEqual(TEXT("Legacy lobby is adopted without recreation"), UI->ShowScreen(TEXT("Lobby"), ExistingLobby), ExistingLobby);
				TestEqual(TEXT("Managed lobby is the existing instance"), UI->GetScreen(TEXT("Lobby")), ExistingLobby);
				UI->CloseScreen(TEXT("Lobby"), false);
				TestFalse(TEXT("Transition removes the managed lobby"), ExistingLobby->IsInViewport());
				UI->OpenScreen(TEXT("Menu"), UASInGameMenuWidget::StaticClass());
				UUserWidget* Result = CreateWidget<UUserWidget>(Player, UASInGameMenuWidget::StaticClass());
				UASUIManagerSubsystem::ShowResultScreen(Result);
				TestNull(TEXT("Result replaces the open menu"), UI->GetScreen(TEXT("Menu")));
				TestEqual(TEXT("Result is managed"), UI->GetScreen(TEXT("GameResult")), Result);
				Player->bShowMouseCursor = false;
				UI->CloseAllScreens();
				TestFalse(TEXT("Closing result does not restore gameplay or override external cursor changes"), Player->bShowMouseCursor);
			}
		}
	}
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
