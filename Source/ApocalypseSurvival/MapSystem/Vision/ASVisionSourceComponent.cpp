#include "MapSystem/Vision/ASVisionSourceComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"
#include "Net/UnrealNetwork.h"

UASVisionSourceComponent::UASVisionSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UASVisionSourceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UASVisionSourceComponent, bVisionActive);
}

void UASVisionSourceComponent::SetVisionActive(bool bActive)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bVisionActive = bActive;
	}
}

void UASVisionSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		Subsystem->RegisterVisionSource(this);
	}

	// 팀 시야 공유에는 모든 팀원의 머신에 모든 소스가 필요하다.
	AActor* Owner = GetOwner();
	if (bMakeOwnerAlwaysRelevant && Owner && Owner->HasAuthority())
	{
		Owner->bAlwaysRelevant = true;
	}
}

void UASVisionSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		Subsystem->UnregisterVisionSource(this);
	}

	Super::EndPlay(EndPlayReason);
}

bool UASVisionSourceComponent::IsProvidingVision(const FGameplayTag& DeadStateTag) const
{
	const AActor* Owner = GetOwner();
	if (!bVisionActive || !IsValid(Owner) || Owner->IsActorBeingDestroyed() || Owner->IsHidden())
	{
		return false;
	}

	if (DeadStateTag.IsValid())
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);

		// 플레이어는 보통 PlayerState에 어빌리티 시스템을 둔다.
		if (!AbilitySystem)
		{
			if (const APawn* Pawn = Cast<APawn>(Owner))
			{
				AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn->GetPlayerState());
			}
		}

		if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(DeadStateTag))
		{
			return false;
		}
	}

	return true;
}
