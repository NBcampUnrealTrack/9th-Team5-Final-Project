#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASLeaveRoomConfirmationWidget.generated.h"

class UButton;

UCLASS()
class APOCALYPSESURVIVAL_API UASLeaveRoomConfirmationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UASLeaveRoomConfirmationWidget(const FObjectInitializer& ObjectInitializer);
	void SetReturnFocusWidget(UUserWidget* Widget) { ReturnFocusWidget = Widget; }
	FSimpleDelegate OnConfirmed;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void Confirm();
	UFUNCTION()
	void Cancel();
	TWeakObjectPtr<UUserWidget> ReturnFocusWidget;
	bool bResolved = false;
};
