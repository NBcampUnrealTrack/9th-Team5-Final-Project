#pragma once

#include "CoreMinimal.h"
#include "Items/Data/ASItemGrant.h"

struct FASLootChestPresetRow;

/** Pure generation: no actor/world access, inventory mutation, global RNG, or asset loading.
 * Call on the server before spawning. A failed generation leaves OutItems empty and Random unchanged.
 */
class APOCALYPSESURVIVAL_API FASLootGenerator
{
public:
	static bool Generate(const FASLootChestPresetRow& Preset, FRandomStream& Random,
		TArray<FASItemGrant>& OutItems, FString& OutError);
};
