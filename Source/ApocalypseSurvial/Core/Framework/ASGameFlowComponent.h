#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASGameFlowComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class APOCALYPSESURVIAL_API UASGameFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASGameFlowComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
};
