#pragma once
#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "GameplayTagContainer.h"
#include "ASQuestArea.generated.h"

/** Reports personal entry to the authored quest target on the server. */
UCLASS()
class APOCALYPSESURVIVAL_API AASQuestArea : public ATriggerBox
{
	GENERATED_BODY()
public:
	AASQuestArea();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (Categories = "Quest.Target"))
	FGameplayTag TargetTag;
private:
	UFUNCTION()
	void HandleEntered(AActor* OverlappedActor, AActor* OtherActor);
};
