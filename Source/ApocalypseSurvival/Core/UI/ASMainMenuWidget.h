#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASMainMenuWidget.generated.h"

class UASSessionSubsystem;
class UTextBlock;
class UWidgetSwitcher;
class UButton;

UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Session")
	void HostRoom(const FString& RoomName, bool bUsePassword, const FString& Password);

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bIsHosting = false;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Menu", meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ScreenSwitcher;
	UPROPERTY(BlueprintReadOnly, Category = "Session", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ConnectionStatusText;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Session")
	void OnHostRoomFailed(const FText& Message);

	UPROPERTY(EditDefaultsOnly, Category = "Session", meta = (ClampMin = "2"))
	int32 PublicConnections = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Session")
	FString LobbyMapPath = TEXT("/Game/ApocalypseSurvival/Maps/Lobby_Proto");

private:
	void SearchActiveJoinRoom();
	FTimerHandle JoinRoomSearchTimer;
	UFUNCTION()
	void HandleSessionCreated(bool bSuccess);
	UFUNCTION()
	void HandleSessionJoined(bool bSuccess, const FString& ErrorMessage);

	void FailHosting(const FText& Message);
	UFUNCTION()
	void ValidateRoomInput();
	TWeakObjectPtr<UUserWidget> CreateRoomPanel;
	UPROPERTY(Transient)
	TObjectPtr<UButton> CreateRoomValidationButton;

	UPROPERTY(Transient)
	TObjectPtr<UASSessionSubsystem> SessionSubsystem;
};
