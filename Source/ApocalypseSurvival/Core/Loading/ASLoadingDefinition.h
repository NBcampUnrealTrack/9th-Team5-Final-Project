#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ASLoadingDefinition.generated.h"

/** Map references remain soft; selecting this definition does not load either world. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASLoadingDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Maps")
	TSoftObjectPtr<UWorld> LoadingMap;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Maps")
	TSoftObjectPtr<UWorld> DefaultGameMap;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading", meta=(ClampMin="1", Units="s"))
	float TimeoutSeconds = 120.f;
};
