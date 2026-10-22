#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "ASJoinRoomWidget.generated.h"

class UButton;
class UComboBoxString;
class UTextBlock;

UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASJoinRoomWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Session")
	void SearchRooms();
	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinSelectedRoom();
	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinRoom(int32 ResultIndex);
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	TArray<FASSessionSearchResult> AvailableRooms;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UFUNCTION(BlueprintImplementableEvent, Category = "Session")
	void OnRoomListUpdated(bool bSuccess);
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SearchRoomsButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> JoinRoomButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> RoomComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RoomStatusText;

private:
	UFUNCTION()
	void HandleSessionsFound(bool bSuccess, const TArray<FASSessionSearchResult>& Results);
	UFUNCTION()
	void HandleSessionJoined(bool bSuccess, const FString& ErrorMessage);
	void SetStatus(const FString& Message);
	void UpdateButtons();
	bool bSearching = false;
	bool bJoining = false;
	UPROPERTY(Transient)
	TObjectPtr<UASSessionSubsystem> SessionSubsystem;
};
