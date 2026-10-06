#pragma once

#include "CoreMinimal.h"
#include "ASItemGrant.generated.h"

class UASItemDefinition;

/** Designer-authored instruction describing an item and quantity to create. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASItemGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UASItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1", UIMin = "1"))
	int32 Quantity = 1;
};
