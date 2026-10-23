#include "Mass/Monster/Targeting/ASMonsterTargetableComponent.h"

#include "Mass/Monster/Subsystems/ASMonsterTargetSubsystem.h"
#include "Mass/Monster/Targeting/ASMonsterTargetable.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMonsterTargetableComponent)

UASMonsterTargetableComponent::UASMonsterTargetableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UASMonsterTargetableComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithTargetSubsystem();
}

void UASMonsterTargetableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromTargetSubsystem();
	Super::EndPlay(EndPlayReason);
}

void UASMonsterTargetableComponent::SetTargetingEnabled(const bool bInTargetingEnabled)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bTargetingEnabled == bInTargetingEnabled)
	{
		return;
	}

	bTargetingEnabled = bInTargetingEnabled;
	if (bTargetingEnabled)
	{
		RegisterWithTargetSubsystem();
	}
	else
	{
		UnregisterFromTargetSubsystem();
	}
}

void UASMonsterTargetableComponent::ConfigureTargetProfile(
	const EASMonsterTargetCapability InCapabilities,
	const float InBaseScore,
	const float InTargetRadius)
{
	TargetCapabilityMask = static_cast<int32>(InCapabilities);
	BaseTargetScore = InBaseScore;
	TargetRadius = FMath::Max(0.0f, InTargetRadius);
}

bool UASMonsterTargetableComponent::IsTargetable() const
{
	const AActor* Owner = GetOwner();
	if (!bTargetingEnabled || !IsValid(Owner) || Owner->IsActorBeingDestroyed())
	{
		return false;
	}

	if (Owner->GetClass()->ImplementsInterface(UASMonsterTargetable::StaticClass()))
	{
		return IASMonsterTargetable::Execute_IsMonsterTargetable(const_cast<AActor*>(Owner));
	}

	return true;
}

FVector UASMonsterTargetableComponent::GetTargetLocation() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return FVector::ZeroVector;
	}

	if (Owner->GetClass()->ImplementsInterface(UASMonsterTargetable::StaticClass()))
	{
		return IASMonsterTargetable::Execute_GetMonsterTargetLocation(const_cast<AActor*>(Owner));
	}

	return Owner->GetActorLocation();
}

float UASMonsterTargetableComponent::GetTargetRadius() const
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->GetClass()->ImplementsInterface(UASMonsterTargetable::StaticClass()))
	{
		return FMath::Max(0.0f, IASMonsterTargetable::Execute_GetMonsterTargetRadius(const_cast<AActor*>(Owner)));
	}

	return TargetRadius;
}

EASMonsterTargetCapability UASMonsterTargetableComponent::GetTargetCapabilities() const
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->GetClass()->ImplementsInterface(UASMonsterTargetable::StaticClass()))
	{
		return IASMonsterTargetable::Execute_GetMonsterTargetCapabilities(const_cast<AActor*>(Owner));
	}

	return static_cast<EASMonsterTargetCapability>(TargetCapabilityMask);
}

float UASMonsterTargetableComponent::GetTargetBaseScore() const
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->GetClass()->ImplementsInterface(UASMonsterTargetable::StaticClass()))
	{
		return IASMonsterTargetable::Execute_GetMonsterTargetBaseScore(const_cast<AActor*>(Owner));
	}

	return BaseTargetScore;
}

void UASMonsterTargetableComponent::RegisterWithTargetSubsystem()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !bTargetingEnabled || TargetHandle.IsValid())
	{
		return;
	}

	if (UASMonsterTargetSubsystem* TargetSubsystem = GetTargetSubsystem())
	{
		TargetHandle = TargetSubsystem->RegisterTarget(*this);
	}
}

void UASMonsterTargetableComponent::UnregisterFromTargetSubsystem()
{
	if (!TargetHandle.IsValid())
	{
		return;
	}

	if (UASMonsterTargetSubsystem* TargetSubsystem = GetTargetSubsystem())
	{
		TargetSubsystem->UnregisterTarget(TargetHandle);
	}

	TargetHandle = FASTargetHandle();
}

UASMonsterTargetSubsystem* UASMonsterTargetableComponent::GetTargetSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UASMonsterTargetSubsystem>() : nullptr;
}

