#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASQuestDialogueComponent.generated.h"

class AASQuestNpc;
class UASQuestComponent;
class UASQuestDefinition;
class UASQuestDialogueWidget;

UENUM()
enum class EASQuestDialogueMode : uint8
{
	None,
	Idle,
	Offer,
	Reminder,
	Report
};

/** Per-player conversation session and owning-client presentation; quest state stays in ASQuestComponent. */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASQuestDialogueComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UASQuestDialogueComponent();
	/** Called only by a validated, authoritative NPC interaction. */
	void BeginDialogue(AASQuestNpc* Npc);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Quest|Dialogue")
	void ServerAdvanceDialogue();
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Quest|Dialogue")
	void ServerCloseDialogue();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, Category = "Quest|Dialogue")
	TSubclassOf<UASQuestDialogueWidget> DialogueWidgetClass;

private:
	UASQuestComponent* GetQuests() const;
	bool IsDialogueValid() const;
	const TArray<FText>& GetDialogueLines() const;
	void SendDialogueLine();
	void CloseDialogue();
	UFUNCTION(Client, Reliable)
	void ClientShowDialogue(const FText& Speaker, const FText& Line, const FText& ContinueLabel);
	UFUNCTION(Client, Reliable)
	void ClientCloseDialogue();

	TWeakObjectPtr<AASQuestNpc> DialogueNpc;
	TWeakObjectPtr<APawn> DialoguePawn;
	UPROPERTY(Transient)
	TObjectPtr<UASQuestDefinition> DialogueQuest;
	UPROPERTY(Transient)
	TObjectPtr<UASQuestDialogueWidget> DialogueWidget;
	EASQuestDialogueMode DialogueMode = EASQuestDialogueMode::None;
	int32 DialogueLine = 0;
};
