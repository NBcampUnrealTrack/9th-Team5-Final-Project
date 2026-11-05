#include "Quests/World/ASQuestArea.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Quests/ASQuestComponent.h"
#include "Quests/Data/ASQuestTags.h"

AASQuestArea::AASQuestArea()
{
	OnActorBeginOverlap.AddDynamic(this, &ThisClass::HandleEntered);
}

void AASQuestArea::HandleEntered(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !TargetTag.IsValid()) return;
	const APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerState* State = Pawn ? Pawn->GetPlayerState() : nullptr;
	UASQuestComponent* Quests = State ? State->FindComponentByClass<UASQuestComponent>() : nullptr;
	if (!Quests) return;
	FASQuestEvent Event;
	Event.EventTag = ASGameplayTags::Quest_Event_ZoneEntered;
	Event.TargetTag = TargetTag;
	Quests->HandleQuestEvent(Event);
}
