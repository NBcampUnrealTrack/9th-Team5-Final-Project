#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASQuestDialogueWidget.generated.h"

class UASQuestDialogueComponent;
class UTextBlock;

/** Functional temporary dialogue screen; the NPC and quest data do not depend on a lobby map. */
UCLASS()
class APOCALYPSESURVIVAL_API UASQuestDialogueWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ShowLine(UASQuestDialogueComponent* InDialogue, const FText& Speaker, const FText& Line, const FText& ContinueLabel);
protected:
	virtual void NativeOnInitialized() override;
private:
	UFUNCTION()
	void Continue();
	UFUNCTION()
	void Close();
	TWeakObjectPtr<UASQuestDialogueComponent> Dialogue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeakerText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LineText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ButtonText;
};
