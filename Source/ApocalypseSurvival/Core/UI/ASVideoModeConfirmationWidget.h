#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASVideoModeConfirmationWidget.generated.h"

class UTextBlock;

UCLASS()
class APOCALYPSESURVIVAL_API UASVideoModeConfirmationWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UASVideoModeConfirmationWidget(const FObjectInitializer& ObjectInitializer);
	void SetSettingsOwner(UUserWidget* Widget) { SettingsOwner = Widget; }
	FSimpleDelegate OnClosed;
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
private:
	UFUNCTION()
	void Confirm();
	UFUNCTION()
	void Revert();
	void Resolve(bool bKeep);
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountdownText;
	TWeakObjectPtr<UUserWidget> SettingsOwner;
	double Deadline = 0.0;
	bool bResolved = false;
};
