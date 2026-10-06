#include "Core/Framework/ASGameFlowComponent.h"

UASGameFlowComponent::UASGameFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UASGameFlowComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UASGameFlowComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

