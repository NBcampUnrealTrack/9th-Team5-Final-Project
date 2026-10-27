#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASInGameMenuWidget.generated.h"

class UButton;
class UWidgetSwitcher;
class UASSettingsWidget;

UCLASS()
class APOCALYPSESURVIVAL_API UASInGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UASInGameMenuWidget(const FObjectInitializer& ObjectInitializer);
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ResumeGame();
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void OpenSettings();
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowMainMenu();
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void LeaveRoom();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResumeButton;
	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;
	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> LeaveRoomButton;
	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> MenuScreenSwitcher;

private:
	UPROPERTY(Transient)
	TObjectPtr<UASSettingsWidget> SettingsPanel;
	bool bLeavingRoom = false;
};
