#include "Map/Vision/ASVisionFunctionLibrary.h"

#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Map/Vision/ASVisibilityComponent.h"
#include "Map/Vision/ASVisionSourceComponent.h"

int32 UASVisionFunctionLibrary::GetLocalTeamId(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const APlayerController* LocalController = nullptr;
	if (World)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (const APlayerController* Controller = It->Get(); Controller && Controller->IsLocalController())
			{
				LocalController = Controller;
				break;
			}
		}
	}
	if (const APawn* Pawn = LocalController ? LocalController->GetPawn() : nullptr)
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
