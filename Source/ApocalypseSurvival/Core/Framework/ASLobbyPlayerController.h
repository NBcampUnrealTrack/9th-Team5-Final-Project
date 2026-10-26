#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ASLobbyPlayerController.generated.h"

class UUserWidget;
class UASCharacterSelectWidget;
class AASLobbyGameState;
class AGameStateBase;

UCLASS()
class APOCALYPSESURVIVAL_API AASLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetReady(bool bReady);
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void StartLobbyGame();
	bool IsReady() const { return bReadyOnServer; }
	UFUNCTION(BlueprintCallable, Category = "CharacterSelect")
	void SelectLobbyCharacter(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "CharacterSelect")
	void SetCharacterReady(bool bReady);
	int32 GetCharacterIndex() const { return CharacterIndexOnServer; }
	bool IsCharacterReady() const { return bCharacterReadyOnServer; }
	void InitializeCharacterSelection(int32 DefaultIndex);
	void SetCharacterSelectionOnServer(int32 Index, bool bReady);

protected:
	virtual void BeginPlay() override;
	virtual void ReceivedPlayer() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|UI")
	TSubclassOf<UUserWidget> LobbyWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|UI")
	TSubclassOf<UASCharacterSelectWidget> CharacterSelectWidgetClass;

private:
	void InitializeLobbyUI();
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);
	UFUNCTION(Server, Reliable)
	void ServerStartLobbyGame();
	UFUNCTION(Server, Reliable)
	void ServerSelectLobbyCharacter(int32 Index);
	UFUNCTION(Server, Reliable)
	void ServerSetCharacterReady(bool bReady);
	UFUNCTION()
	void HandleLobbyPhaseChanged();
	void HandleGameStateSet(AGameStateBase* State);
	void ShowWidget(UUserWidget* Widget);
	void RemoveOwnedWidgets(TSubclassOf<UUserWidget> WidgetClass);
	void ClearLobbyScreens();
	int32 CharacterIndexOnServer = INDEX_NONE;
	bool bCharacterReadyOnServer = false;
	UPROPERTY(Transient)
	TObjectPtr<AASLobbyGameState> BoundLobbyState;
	UPROPERTY(Transient)
	TObjectPtr<UASCharacterSelectWidget> CharacterSelectWidget;
	bool bReadyOnServer = false;
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LobbyWidget;
};
