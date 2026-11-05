#include "Quests/ASQuestEventLibrary.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Quests/ASQuestComponent.h"
#include "Quests/Data/ASQuestTags.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void FASQuestEventLibrary::ReportMonsterDefeated(UWorld* World, UASMonsterDefinition* Definition, AController* Killer)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	FASQuestEvent Event;
	Event.EventTag = ASGameplayTags::Quest_Event_EnemyKilled;
	Event.MonsterDefinition = Definition;
	Event.KillerPlayerState = IsValid(Killer) ? Killer->GetPlayerState<APlayerState>() : nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Player = It->Get();
		APlayerState* State = Player ? Player->PlayerState.Get() : nullptr;
		if (UASQuestComponent* Quests = State ? State->FindComponentByClass<UASQuestComponent>() : nullptr)
			Quests->HandleQuestEvent(Event);
	}
}
