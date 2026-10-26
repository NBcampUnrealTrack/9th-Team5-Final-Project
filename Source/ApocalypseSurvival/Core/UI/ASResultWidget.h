#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASResultWidget.generated.h"

class UButton;

UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Session")
	void ReturnToLobby();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Session", meta = (BindWidget))
	TObjectPtr<UButton> MainMenuButton;
};
