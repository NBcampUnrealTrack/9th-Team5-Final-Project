#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Framework/ASLobbyGameState.h"
#include "ASLobbyWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASLobbyWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void ToggleReady();
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestStartGame();
	UFUNCTION(BlueprintPure, Category = "Lobby")
	AASLobbyGameState* GetLobbyState() const;
	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsLocalHost() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void OnLobbyUpdated();
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartGameButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerListText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LobbyStatusText;

private:
	UFUNCTION()
	void HandleLobbyUpdated();
	void HandleGameStateSet(AGameStateBase* State);
	UPROPERTY(Transient)
	TObjectPtr<AASLobbyGameState> BoundState;
};
