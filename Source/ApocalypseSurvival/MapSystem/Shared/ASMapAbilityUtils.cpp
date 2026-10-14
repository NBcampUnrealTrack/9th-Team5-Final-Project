#include "MapSystem/Shared/ASMapAbilityUtils.h"

#include "AbilitySystemComponent.h"
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

		// 소유자의 인터페이스 포인터 초기화 전에도 액터가 직접 소유한 ASC 컴포넌트를 찾는다.
		if (IsValid(Actor))
		{
			if (UAbilitySystemComponent* AbilitySystem = Actor->FindComponentByClass<UAbilitySystemComponent>())
			{
				return AbilitySystem;
			}
		}

		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn->GetPlayerState());
		}

		return nullptr;
	}
}
