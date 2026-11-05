#include "Quests/Dialogue/ASQuestDialogueComponent.h"
#include "Quests/ASQuestComponent.h"
#include "Quests/Data/ASQuestDefinition.h"
#include "Quests/Dialogue/ASQuestNpc.h"
#include "Quests/Dialogue/ASQuestDialogueWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

UASQuestDialogueComponent::UASQuestDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DialogueWidgetClass = UASQuestDialogueWidget::StaticClass();
}

UASQuestComponent* UASQuestDialogueComponent::GetQuests() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UASQuestComponent>() : nullptr;
}

void UASQuestDialogueComponent::BeginDialogue(AASQuestNpc* Npc)
{
	const APlayerState* State = Cast<APlayerState>(GetOwner());
	APlayerController* PC = State ? State->GetPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UASQuestComponent* Quests = GetQuests();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Quests || !IsValid(Npc) || !Npc->CanInteract_Implementation(Pawn)) return;
	CloseDialogue();
	DialogueNpc = Npc;
	DialoguePawn = Pawn;
	DialogueQuest = Quests->FindQuestForNpc(Npc->GetNpcId());
	DialogueLine = 0;
	if (!DialogueQuest)
	{
		DialogueMode = EASQuestDialogueMode::Idle;
		ClientShowDialogue(Npc->GetDisplayName(), Npc->GetIdleDialogue(), NSLOCTEXT("Quests", "CloseDialogue", "대화 종료"));
		return;
	}
	FASQuestProgress Progress;
	const bool bAccepted = Quests->GetQuestProgress(DialogueQuest->QuestId, Progress);
	DialogueMode = !bAccepted ? EASQuestDialogueMode::Offer
		: Progress.Status == EASQuestStatus::ReadyToTurnIn ? EASQuestDialogueMode::Report : EASQuestDialogueMode::Reminder;
	SendDialogueLine();
}

const TArray<FText>& UASQuestDialogueComponent::GetDialogueLines() const
{
	if (DialogueMode == EASQuestDialogueMode::Offer) return DialogueQuest->OfferDialogue;
	if (DialogueMode == EASQuestDialogueMode::Report) return DialogueQuest->CompletionDialogue;
	return DialogueQuest->ProgressDialogue;
}

bool UASQuestDialogueComponent::IsDialogueValid() const
{
	const APlayerState* State = Cast<APlayerState>(GetOwner());
	const APlayerController* PC = State ? State->GetPlayerController() : nullptr;
	const UASQuestComponent* Quests = GetQuests();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Quests || !PC || !DialogueNpc.IsValid()
		|| !DialoguePawn.IsValid() || PC->GetPawn() != DialoguePawn.Get()
		|| !DialogueNpc->CanInteract_Implementation(DialoguePawn.Get())) return false;
	if (DialogueMode == EASQuestDialogueMode::Idle) return true;
	if (!DialogueQuest || DialogueMode == EASQuestDialogueMode::None) return false;
	const FName NpcId = DialogueNpc->GetNpcId();
	if (DialogueMode == EASQuestDialogueMode::Offer)
		return !Quests->IsAutomaticQuest(DialogueQuest) && DialogueQuest->GetOfferNpcId() == NpcId && Quests->CanAcceptQuest(DialogueQuest);
	if (DialogueMode == EASQuestDialogueMode::Report && DialogueQuest->TurnInNpcId != NpcId) return false;
	if (DialogueMode == EASQuestDialogueMode::Reminder && DialogueQuest->GetOfferNpcId() != NpcId && DialogueQuest->TurnInNpcId != NpcId) return false;
	FASQuestProgress Progress;
	return Quests->GetQuestProgress(DialogueQuest->QuestId, Progress)
		&& Progress.Status == (DialogueMode == EASQuestDialogueMode::Report ? EASQuestStatus::ReadyToTurnIn : EASQuestStatus::InProgress);
}

void UASQuestDialogueComponent::SendDialogueLine()
{
	if (!IsDialogueValid() || !GetDialogueLines().IsValidIndex(DialogueLine)) { CloseDialogue(); return; }
	const bool bLast = DialogueLine == GetDialogueLines().Num() - 1;
	const FText Label = !bLast ? NSLOCTEXT("Quests", "NextDialogue", "다음")
		: DialogueMode == EASQuestDialogueMode::Offer ? NSLOCTEXT("Quests", "AcceptQuest", "퀘스트 수락")
		: DialogueMode == EASQuestDialogueMode::Report ? NSLOCTEXT("Quests", "ReportQuest", "퀘스트 완료")
		: NSLOCTEXT("Quests", "CloseDialogue", "대화 종료");
	ClientShowDialogue(DialogueNpc->GetDisplayName(), GetDialogueLines()[DialogueLine], Label);
}

void UASQuestDialogueComponent::ServerAdvanceDialogue_Implementation()
{
	if (!IsDialogueValid() || DialogueMode == EASQuestDialogueMode::Idle) { CloseDialogue(); return; }
	if (++DialogueLine < GetDialogueLines().Num()) { SendDialogueLine(); return; }
	// Only the last explicit click commits. Closing and invalid sessions cannot accept or report.
	if (DialogueMode == EASQuestDialogueMode::Offer) GetQuests()->AcceptQuest(DialogueQuest);
	else if (DialogueMode == EASQuestDialogueMode::Report) GetQuests()->ClaimRewardInternal(DialogueQuest->QuestId, DialogueNpc->GetNpcId());
	CloseDialogue();
}

void UASQuestDialogueComponent::CloseDialogue()
{
	DialogueNpc.Reset();
	DialoguePawn.Reset();
	DialogueQuest = nullptr;
	DialogueLine = 0;
	DialogueMode = EASQuestDialogueMode::None;
	ClientCloseDialogue();
}

void UASQuestDialogueComponent::ServerCloseDialogue_Implementation()
{
	CloseDialogue();
}

void UASQuestDialogueComponent::ClientShowDialogue_Implementation(const FText& Speaker, const FText& Line, const FText& ContinueLabel)
{
	const APlayerState* State = Cast<APlayerState>(GetOwner());
	APlayerController* PC = State ? State->GetPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer()) return;
	if (!DialogueWidget) DialogueWidget = CreateWidget<UASQuestDialogueWidget>(PC, DialogueWidgetClass);
	if (!DialogueWidget) return;
	if (!DialogueWidget->IsInViewport()) DialogueWidget->AddToViewport(50);
	DialogueWidget->ShowLine(this, Speaker, Line, ContinueLabel);
}

void UASQuestDialogueComponent::ClientCloseDialogue_Implementation()
{
	if (DialogueWidget) DialogueWidget->RemoveFromParent();
	DialogueWidget = nullptr;
}

void UASQuestDialogueComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DialogueWidget) DialogueWidget->RemoveFromParent();
	Super::EndPlay(EndPlayReason);
}
