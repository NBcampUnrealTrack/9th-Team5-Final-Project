#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Items/Data/ASItemGrant.h"
#include "ASStartingItemsRow.generated.h"

/** Fixed inventory contents granted once when a player's inventory begins play. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASStartingItemsRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Array order determines backpack order; repeated definitions share stacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FASItemGrant> Items;
};
