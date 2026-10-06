#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ASInteractable.generated.h"

UINTERFACE(BlueprintType)
class APOCALYPSESURVIVAL_API UASInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Contract used by player interaction code without depending on concrete actor classes. */
class APOCALYPSESURVIVAL_API IASInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FVector GetInteractionLocation() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	float GetInteractionRange() const;
};
