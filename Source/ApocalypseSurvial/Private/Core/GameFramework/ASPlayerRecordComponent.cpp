#include "Core/GameFramework/ASPlayerRecordComponent.h"

UASPlayerRecordComponent::UASPlayerRecordComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UASPlayerRecordComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UASPlayerRecordComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

