#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ASQuestSettings.generated.h"

class UASQuestCatalog;

/** Project Settings > Game > Quests. Content references belong in config and assets, not native constructors. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Quests"))
class APOCALYPSESURVIVAL_API UASQuestSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	UPROPERTY(Config, EditAnywhere, Category = "Data")
	TSoftObjectPtr<UASQuestCatalog> Catalog;
};
