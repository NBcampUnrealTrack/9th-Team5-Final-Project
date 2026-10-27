#include "Core/Cheats/ASCheatManager.h"

#include "Core/Framework/ASPlayerController.h"

void UASCheatManager::DebugGod()
{
#if !UE_BUILD_SHIPPING
	if (AASPlayerController* PlayerController = Cast<AASPlayerController>(GetOuter()))
	{
		PlayerController->ServerDebugGod();
	}
#endif
}
