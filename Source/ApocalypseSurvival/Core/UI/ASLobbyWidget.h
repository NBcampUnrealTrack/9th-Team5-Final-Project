#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "ASLobbyWidget.generated.h"

class UButton;
class UTextBlock;
class UASLeaveRoomConfirmationWidget;

UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASLobbyWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void ToggleReady();
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestStartGame();
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void LeaveRoom();
	UFUNCTION(BlueprintCallable, Category = "Lobby|Steam")
	void InviteFriends();
	UFUNCTION(BlueprintPure, Category = "Lobby")
	AASLobbyGameState* GetLobbyState() const;
	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsLocalHost() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void OnLobbyUpdated();
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartGameButton;
	UPROPERTY(BlueprintReadOnly, Category = "Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UButton> LeaveRoomButton;
	UPROPERTY(BlueprintReadOnly, Category = "Lobby|Steam", meta = (BindWidgetOptional))
	TObjectPtr<UButton> InviteFriendsButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerListText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LobbyStatusText;
	UPROPERTY(BlueprintReadOnly, Category = "Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RoomCodeText;

private:
	void ConfirmLeaveRoom();
	UPROPERTY(Transient)
	TObjectPtr<UASLeaveRoomConfirmationWidget> LeaveConfirmation;
	UFUNCTION()
	void HandleLobbyUpdated();
	void HandleGameStateSet(AGameStateBase* State);
	UPROPERTY(Transient)
	TObjectPtr<AASLobbyGameState> BoundState;
};
