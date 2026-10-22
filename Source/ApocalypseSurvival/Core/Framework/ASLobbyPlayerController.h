#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ASLobbyPlayerController.generated.h"

class UUserWidget;

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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|UI")
	TSubclassOf<UUserWidget> LobbyWidgetClass;

private:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);
	UFUNCTION(Server, Reliable)
	void ServerStartLobbyGame();
	bool bReadyOnServer = false;
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LobbyWidget;
};
