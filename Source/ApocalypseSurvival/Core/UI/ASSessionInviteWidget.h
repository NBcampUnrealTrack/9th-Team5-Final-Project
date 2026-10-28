#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASSessionInviteWidget.generated.h"

class UEditableTextBox;
class UTextBlock;

// Native so Steam invitations work with the existing menu Blueprint without asset changes.
UCLASS()
class APOCALYPSESURVIVAL_API UASSessionInviteWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UASSessionInviteWidget(const FObjectInitializer& ObjectInitializer);
	void Configure(const FString& RoomName, bool bRequiresPassword);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void Confirm();
	UFUNCTION()
	void Cancel();
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> PasswordInput;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Message;
	bool bPasswordRequired = false;
};
