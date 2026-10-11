#include "MapSystem/Shared/ASMapAbilityUtils.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

namespace ASMapAbilityUtils
{
	UAbilitySystemComponent* FindAbilitySystemComponent(const AActor* Actor)
	{
		if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
		{
			return AbilitySystem;
		}

		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn->GetPlayerState());
		}

		return nullptr;
	}
}
