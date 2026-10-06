#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ASRespawnPoint.generated.h"

class UArrowComponent;

UCLASS()
class APOCALYPSESURVIVAL_API AASRespawnPoint : public AActor
{
	GENERATED_BODY()
public:
	AASRespawnPoint();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn")
	FText DisplayName;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn")
	FGameplayTag ZoneTag;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UArrowComponent> Arrow;
#endif
};
