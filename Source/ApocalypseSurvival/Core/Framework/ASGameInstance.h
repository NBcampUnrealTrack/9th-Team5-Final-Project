#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "ASGameInstance.generated.h"

class UASLoadingDefinition;

UCLASS()
class APOCALYPSESURVIVAL_API UASGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	/** Assign the project's loading definition in the GameInstance Blueprint defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loading")
	TObjectPtr<UASLoadingDefinition> LoadingDefinition;
};
