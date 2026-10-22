#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASMainMenuWidget.generated.h"

class UASSessionSubsystem;

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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Session")
	void OnHostRoomFailed(const FText& Message);

	UPROPERTY(EditDefaultsOnly, Category = "Session", meta = (ClampMin = "2"))
	int32 PublicConnections = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Session")
	FString LobbyMapPath = TEXT("/Game/ApocalypseSurvival/Maps/Lobby_Proto");

private:
	UFUNCTION()
	void HandleSessionCreated(bool bSuccess);

	void FailHosting(const FText& Message);

	UPROPERTY(Transient)
	TObjectPtr<UASSessionSubsystem> SessionSubsystem;
};
