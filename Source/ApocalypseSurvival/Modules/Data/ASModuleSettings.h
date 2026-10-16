#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ASModuleSettings.generated.h"

class UASModuleDefinition;

/** Register module assets in Project Settings > Game > Modules. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Modules"))
class APOCALYPSESURVIVAL_API UASModuleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Loadout", meta = (ClampMin = "1", UIMin = "1"))
	int32 SlotCount = 3;

	UPROPERTY(Config, EditAnywhere, Category = "Catalog")
	TArray<TSoftObjectPtr<UASModuleDefinition>> Modules;

	UPROPERTY(Config, EditAnywhere, Category = "Save")
	FString SaveSlotPrefix = TEXT("ModuleProgression");

	int32 GetSlotCount() const { return FMath::Max(1, SlotCount); }
	UASModuleDefinition* FindModule(FName ModuleId, FString& OutError) const;
	bool ResolveLoadout(const TArray<FName>& ModuleIds, TArray<UASModuleDefinition*>& OutDefinitions, FString& OutError) const;
};
