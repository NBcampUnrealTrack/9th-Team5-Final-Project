#pragma once

#include "CoreMinimal.h"
#include "Items/Data/ASItemGrant.h"

class UASItemDefinition;

/** Shared rules for direct grants, preset grants, and chest initialization. */
class APOCALYPSESURVIVAL_API FASLootValidation
{
public:
	static bool ValidateItem(const UASItemDefinition* Item, FString& OutError);
	static bool ValidateGrants(const TArray<FASItemGrant>& Items, FString& OutError);
};
