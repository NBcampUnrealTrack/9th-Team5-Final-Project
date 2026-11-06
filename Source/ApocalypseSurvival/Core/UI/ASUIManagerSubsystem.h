#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ASUIManagerSubsystem.generated.h"

class UUserWidget;

// Manages modal screens and HUDs for one local player. Gameplay requests remain with their owners.
UCLASS()
class APOCALYPSESURVIVAL_API UASUIManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	UFUNCTION(BlueprintCallable, Category = "UI")
	UUserWidget* OpenScreen(FName ScreenName, TSubclassOf<UUserWidget> WidgetClass, int32 ZOrder = 20);

	UFUNCTION(BlueprintCallable, Category = "UI", meta = (DeterminesOutputType = "WidgetClass"))
	static UUserWidget* OpenScreenForPlayer(APlayerController* OwningPlayer, FName ScreenName,
		TSubclassOf<UUserWidget> WidgetClass, int32 ZOrder = 0);

	// Adopt an existing screen created by a legacy Blueprint without adding a second copy.
	UUserWidget* ShowScreen(FName ScreenName, UUserWidget* Widget, int32 ZOrder = 0, bool bRestoreGameplayOnClose = true);

	UFUNCTION(BlueprintCallable, Category = "UI")
	static void ShowResultScreen(UUserWidget* Widget, int32 ZOrder = 100);

	// Closing a screen also closes the modal screens opened above it.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseScreen(FName ScreenName, bool bRestoreInput = true);

	// Closes modal menus and popups; HUDs remain visible.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseAllScreens(bool bRestoreInput = true);

	UFUNCTION(BlueprintPure, Category = "UI")
	UUserWidget* GetScreen(FName ScreenName) const;

	// HUDs do not participate in the modal stack or change input, cursor, or focus.
	UFUNCTION(BlueprintCallable, Category = "UI|HUD", meta = (DeterminesOutputType = "WidgetClass"))
	UUserWidget* OpenHUD(FName HUDName, TSubclassOf<UUserWidget> WidgetClass, int32 ZOrder = 0);

	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	UUserWidget* ShowHUD(FName HUDName, UUserWidget* Widget, int32 ZOrder = 0);

	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	static void ShowHUDForWidget(UUserWidget* Widget, FName HUDName = TEXT("GameHUD"), int32 ZOrder = 0);

	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	void CloseHUD(FName HUDName);

	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	void CloseAllHUDs();

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	UUserWidget* GetHUD(FName HUDName) const;

private:
	void HandleWidgetDestruct(UUserWidget* Widget);
	void HandleHUDDestruct(UUserWidget* Widget);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UpdateInput();

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UUserWidget>> Screens;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UUserWidget>> HUDs;
	TWeakObjectPtr<APlayerController> HUDController;
	TArray<FName> ScreenOrder;
	TWeakObjectPtr<APlayerController> ScreenController;
	bool bCursorVisibleBeforeScreens = true;
	bool bRestoreGameplayAfterScreens = true;
};
