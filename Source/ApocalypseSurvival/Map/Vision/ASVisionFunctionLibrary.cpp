#include "Map/Vision/ASVisionFunctionLibrary.h"

#include "GameFramework/Pawn.h"
#include "Map/Core/ASMapFunctionLibrary.h"
#include "Map/Vision/ASVisibilityComponent.h"
#include "Map/Vision/ASVisionSourceComponent.h"

int32 UASVisionFunctionLibrary::GetLocalTeamId(const UObject* WorldContextObject)
{
	if (const APawn* Pawn = UASMapFunctionLibrary::GetLocalPlayerPawn(WorldContextObject))
	{
		if (const UASVisionSourceComponent* Source = Pawn->FindComponentByClass<UASVisionSourceComponent>())
		{
			return Source->GetTeamId();
		}
	}
	return 0;
}

void UASVisionFunctionLibrary::NotifyAttackStarted(AActor* Attacker)
{
	if (!Attacker || !Attacker->HasAuthority())
	{
		return;
	}

	if (UASVisibilityComponent* Visibility = Attacker->FindComponentByClass<UASVisibilityComponent>())
	{
		Visibility->NotifyAttackStarted();
	}
}

bool UASVisionFunctionLibrary::IsActorVisibleToLocalTeam(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	const UASVisibilityComponent* Visibility = Actor->FindComponentByClass<UASVisibilityComponent>();
	return !Visibility || Visibility->IsVisibleToLocalTeam();
}
