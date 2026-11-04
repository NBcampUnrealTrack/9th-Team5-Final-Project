#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ASUIManagerSubsystem.generated.h"

class UUserWidget;

// Manages modal screens for one local player. Gameplay and session requests remain with their owners.
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

	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseAllScreens(bool bRestoreInput = true);

	UFUNCTION(BlueprintPure, Category = "UI")
	UUserWidget* GetScreen(FName ScreenName) const;

private:
	void HandleWidgetDestruct(UUserWidget* Widget);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UpdateInput();

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UUserWidget>> Screens;
	TArray<FName> ScreenOrder;
	TWeakObjectPtr<APlayerController> ScreenController;
	bool bCursorVisibleBeforeScreens = true;
	bool bRestoreGameplayAfterScreens = true;
};
