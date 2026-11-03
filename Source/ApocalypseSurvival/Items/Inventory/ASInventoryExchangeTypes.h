#pragma once

#include "CoreMinimal.h"
#include "ASInventoryExchangeTypes.generated.h"

UENUM(BlueprintType)
enum class EASInventoryExchangeResult : uint8
{
	Success,
	InvalidRequest,
	MissingMaterials,
	InventoryFull,
	NoAuthority
};
