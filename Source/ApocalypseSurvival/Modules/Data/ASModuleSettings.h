#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Modules/Data/ASModuleBalanceRows.h"
#include "ASModuleSettings.generated.h"

struct FASResolvedModuleOption
{
	FName OptionId;
	FASModuleOptionBalanceRow Data;
};

/** Two equipment tables, lobby slot count, and reward generation rules. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Modules"))
class APOCALYPSESURVIVAL_API UASModuleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Loadout", meta = (ClampMin = "1", UIMin = "1"))
	int32 SlotCount = 3;

	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/ApocalypseSurvival.ASModuleBalanceRow"))
	TSoftObjectPtr<UDataTable> ModuleBalanceTable;

	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/ApocalypseSurvival.ASModuleOptionBalanceRow"))
	TSoftObjectPtr<UDataTable> OptionBalanceTable;

	UPROPERTY(Config, EditAnywhere, Category = "Save")
	FString SaveSlotPrefix = TEXT("ModuleProgression");

	/** Weights for 0, 1, 2, and 3 options. Leave empty until design sets drop rates. */
	UPROPERTY(Config, EditAnywhere, Category = "Options", meta = (ClampMin = "0.0"))
	TArray<float> OptionCountWeights;

	int32 GetSlotCount() const { return FMath::Max(1, SlotCount); }
	const FASModuleBalanceRow* FindModule(FName ModuleId, FString& OutError) const;
	const FASModuleOptionBalanceRow* FindOption(FName OptionId, FString& OutError) const;
	bool ResolveOptionPool(FName ModuleId, TArray<FASResolvedModuleOption>& OutPool, FString& OutError) const;
	bool ValidateConfiguration(FString& OutError) const;
	bool ResolveInstanceLoadout(const TArray<FASModuleInstanceData>& Instances, FString& OutError) const;
	bool GenerateInstance(FName ModuleId, int32 OptionCount, FRandomStream& Random, FASModuleInstanceData& OutInstance, FString& OutError) const;
	bool GenerateInstanceFromConfiguredWeights(FName ModuleId, FRandomStream& Random, FASModuleInstanceData& OutInstance, FString& OutError) const;

	/** Older rolls resolve their stat from OptionId; new rolls capture stat and value together. */
	bool ResolveOptionStat(const FASModuleOptionRoll& Roll, EASModuleStat& OutStat, FString& OutError) const;
};