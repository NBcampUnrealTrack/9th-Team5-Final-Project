#include "Quests/Dialogue/ASQuestNpc.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Quests/ASQuestComponent.h"
#include "Quests/Dialogue/ASQuestDialogueComponent.h"
#include "Quests/Data/ASQuestNpcDefinition.h"
#include "Quests/Data/ASQuestTags.h"
#include "UObject/ConstructorHelpers.h"

AASQuestNpc::AASQuestNpc()
{
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Shape.Succeeded()) Mesh->SetStaticMesh(Shape.Object);
	Mesh->SetRelativeScale3D(FVector(.6f, .6f, 1.6f));
}

FName AASQuestNpc::GetNpcId() const
{
	return Definition ? Definition->NpcId : NpcId;
}

FText AASQuestNpc::GetDisplayName() const
{
	return Definition ? Definition->DisplayName : DisplayName;
}

FText AASQuestNpc::GetIdleDialogue() const
{
	return Definition ? Definition->IdleDialogue : NSLOCTEXT("Quests", "NoQuest", "지금은 전할 의뢰가 없습니다.");
}

bool AASQuestNpc::CanInteract_Implementation(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerState* State = Pawn ? Pawn->GetPlayerState() : nullptr;
	const UASQuestComponent* Quests = State ? State->FindComponentByClass<UASQuestComponent>() : nullptr;
	return !GetNpcId().IsNone() && Quests && Quests->IsPersistenceReady()
		&& State->FindComponentByClass<UASQuestDialogueComponent>()
		&& FVector::DistSquared(GetActorLocation(), Pawn->GetActorLocation()) <= FMath::Square(InteractionRange);
}

void AASQuestNpc::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract_Implementation(Interactor)) return;
	UASQuestComponent* Quests = Cast<APawn>(Interactor)->GetPlayerState()->FindComponentByClass<UASQuestComponent>();
	if (Definition && Definition->InteractionTargetTag.IsValid())
	{
		FASQuestEvent Event;
		Event.EventTag = ASGameplayTags::Quest_Event_Interacted;
		Event.TargetTag = Definition->InteractionTargetTag;
		Quests->HandleQuestEvent(Event);
	}
	Cast<APawn>(Interactor)->GetPlayerState()->FindComponentByClass<UASQuestDialogueComponent>()->BeginDialogue(this);
}
