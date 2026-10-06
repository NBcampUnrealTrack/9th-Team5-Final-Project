#include "Core/Framework/ASPlayerRecordComponent.h"
#include "Net/UnrealNetwork.h"

UASPlayerRecordComponent::UASPlayerRecordComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UASPlayerRecordComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UASPlayerRecordComponent, DeathCount);
	DOREPLIFETIME(UASPlayerRecordComponent, KillCount);
}

void UASPlayerRecordComponent::AddDeath()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	++DeathCount;
}

void UASPlayerRecordComponent::AddKill()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	++KillCount;
}
