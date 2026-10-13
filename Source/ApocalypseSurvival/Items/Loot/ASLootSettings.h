#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "ASLootSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Loot"))
class APOCALYPSESURVIVAL_API UASLootSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Appearance",
		meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> RarityAppearanceTable;
};
