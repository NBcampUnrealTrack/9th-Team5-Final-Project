#include "Core/UI/ASUIManagerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"

void UASUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
}

void UASUIManagerSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	CloseAllScreens(false);
	Super::Deinitialize();
}

void UASUIManagerSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	if (ScreenController.Get() != NewPlayerController) CloseAllScreens(false);
	Super::PlayerControllerChanged(NewPlayerController);
}

UUserWidget* UASUIManagerSubsystem::GetScreen(const FName ScreenName) const
{
	const TObjectPtr<UUserWidget>* Widget = Screens.Find(ScreenName);
	return Widget && IsValid(Widget->Get()) && (*Widget)->IsInViewport() ? Widget->Get() : nullptr;
}

UUserWidget* UASUIManagerSubsystem::OpenScreen(const FName ScreenName, TSubclassOf<UUserWidget> WidgetClass, const int32 ZOrder)
{
	APlayerController* Player = GetLocalPlayer()->GetPlayerController(GetLocalPlayer()->GetWorld());
	if (ScreenName.IsNone() || !WidgetClass || !Player || !Player->IsLocalController()) return nullptr;
	if (UUserWidget* Existing = GetScreen(ScreenName)) return Existing;
	if (Screens.Contains(ScreenName)) CloseScreen(ScreenName, false);
	UUserWidget* Widget = CreateWidget<UUserWidget>(Player, WidgetClass);
	if (!Widget) return nullptr;
	return ShowScreen(ScreenName, Widget, ZOrder);
}

UUserWidget* UASUIManagerSubsystem::OpenScreenForPlayer(APlayerController* OwningPlayer, const FName ScreenName,
	TSubclassOf<UUserWidget> WidgetClass, const int32 ZOrder)
{
	ULocalPlayer* Local = OwningPlayer ? OwningPlayer->GetLocalPlayer() : nullptr;
	UASUIManagerSubsystem* UI = Local ? Local->GetSubsystem<UASUIManagerSubsystem>() : nullptr;
	return UI ? UI->OpenScreen(ScreenName, WidgetClass, ZOrder) : nullptr;
}

UUserWidget* UASUIManagerSubsystem::ShowScreen(const FName ScreenName, UUserWidget* Widget, const int32 ZOrder, const bool bRestoreGameplayOnClose)
{
	APlayerController* Player = GetLocalPlayer()->GetPlayerController(GetLocalPlayer()->GetWorld());
	if (ScreenName.IsNone() || !IsValid(Widget) || !Player || !Player->IsLocalController()
		|| Widget->GetOwningPlayer() != Player) return nullptr;
	if (UUserWidget* Existing = GetScreen(ScreenName))
	{
		if (Existing != Widget) Widget->RemoveFromParent();
		return Existing;
	}
	if (Screens.Contains(ScreenName)) CloseScreen(ScreenName, false);
	if (ScreenOrder.IsEmpty())
	{
		ScreenController = Player;
		bCursorVisibleBeforeScreens = Player->bShowMouseCursor;
		bRestoreGameplayAfterScreens = bRestoreGameplayOnClose;
	}
	Screens.Add(ScreenName, Widget);
	ScreenOrder.Add(ScreenName);
	Widget->OnNativeDestruct.AddUObject(this, &ThisClass::HandleWidgetDestruct);
	if (!Widget->IsInViewport() && !Widget->AddToPlayerScreen(ZOrder))
	{
		Widget->OnNativeDestruct.RemoveAll(this);
		Screens.Remove(ScreenName);
		ScreenOrder.Remove(ScreenName);
		return nullptr;
	}
	UpdateInput();
	return Widget;
}

void UASUIManagerSubsystem::ShowResultScreen(UUserWidget* Widget, const int32 ZOrder)
{
	ULocalPlayer* Local = Widget ? Widget->GetOwningLocalPlayer() : nullptr;
	UASUIManagerSubsystem* UI = Local ? Local->GetSubsystem<UASUIManagerSubsystem>() : nullptr;
	if (!UI) return;
	if (Widget->GetOwningPlayer() != Local->GetPlayerController(Local->GetWorld())) return;
	if (UUserWidget* Existing = UI->GetScreen(TEXT("GameResult")))
	{
		if (Existing != Widget) Widget->RemoveFromParent();
		return;
	}
	UI->CloseAllScreens(false);
	UI->ShowScreen(TEXT("GameResult"), Widget, ZOrder, false);
}

void UASUIManagerSubsystem::CloseScreen(const FName ScreenName, const bool bRestoreInput)
{
	const int32 Index = ScreenOrder.IndexOfByKey(ScreenName);
	if (Index == INDEX_NONE) return;
	// Detach first: NativeDestruct may remove other widgets while this batch is closing.
	TArray<TObjectPtr<UUserWidget>> Closing;
	for (int32 Position = ScreenOrder.Num() - 1; Position >= Index; --Position)
	{
		if (UUserWidget* Widget = Screens.FindRef(ScreenOrder[Position]))
		{
			Widget->OnNativeDestruct.RemoveAll(this);
			Closing.Add(Widget);
		}
		Screens.Remove(ScreenOrder[Position]);
	}
	ScreenOrder.SetNum(Index);
	for (UUserWidget* Widget : Closing) Widget->RemoveFromParent();
	if (bRestoreInput && (!ScreenOrder.IsEmpty() || bRestoreGameplayAfterScreens)) UpdateInput();
	if (ScreenOrder.IsEmpty()) ScreenController.Reset();
}

void UASUIManagerSubsystem::CloseAllScreens(const bool bRestoreInput)
{
	if (!ScreenOrder.IsEmpty()) CloseScreen(ScreenOrder[0], bRestoreInput);
}

void UASUIManagerSubsystem::HandleWidgetDestruct(UUserWidget* Widget)
{
	FName ScreenName = NAME_None;
	for (const auto& Entry : Screens)
	{
		if (Entry.Value == Widget)
		{
			ScreenName = Entry.Key;
			break;
		}
	}
	if (!ScreenName.IsNone()) CloseScreen(ScreenName);
}

void UASUIManagerSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	for (const auto& Entry : Screens)
	{
		if (Entry.Value && Entry.Value->GetWorld() == World)
		{
			CloseAllScreens(false);
			break;
		}
	}
}

void UASUIManagerSubsystem::UpdateInput()
{
	APlayerController* Player = ScreenController.Get();
	if (!Player || !Player->IsLocalController()) return;
	if (!ScreenOrder.IsEmpty())
	{
		UUserWidget* Top = GetScreen(ScreenOrder.Last());
		if (!Top) return;
		FInputModeUIOnly Mode;
		if (Top->IsFocusable()) Mode.SetWidgetToFocus(Top->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Player->SetInputMode(Mode);
		Player->bShowMouseCursor = true;
	}
	else
	{
		// Restore the project's gameplay input mode after the last managed modal closes.
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		Mode.SetHideCursorDuringCapture(false);
		Player->SetInputMode(Mode);
		Player->bShowMouseCursor = bCursorVisibleBeforeScreens;
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetUserFocusToGameViewport(FMath::Max(0, GetLocalPlayer()->GetControllerId()));
		}
	}
}
