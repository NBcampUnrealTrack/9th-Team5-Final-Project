#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Modules/Data/ASModuleTypes.h"
#include "ASModuleBalanceRows.generated.h"

class UASAbilitySet;
class UTexture2D;

/** Row name = stable ModuleId. Imports preserve editor fields omitted from the sheet. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASModuleBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	EASModuleCategory Category = EASModuleCategory::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Core")
	EASModuleStat CoreStat = EASModuleStat::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Core")
	float CoreValue = 1.0f;

	/** Shared identity prevents equipping the same core twice. Empty uses CoreStat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Core")
	FName CoreEffectId;

	/** Options with this PoolId are eligible. None means no random options. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	FName OptionPoolId;

	/** Only special modules need a set. Simple stat modules need no PDA or individual GE. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special")
	TSoftObjectPtr<UASAbilitySet> AbilitySet;

	FName GetCoreEffectIdentity() const;
	bool Validate(FString& OutError) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** Row name = stable OptionId. Multiple modules can share the same PoolId. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASModuleOptionBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText DisplayName;

	/** None removes the option from drops while retaining its definition for older saves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Option")
	FName PoolId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Option")
	EASModuleStat Stat = EASModuleStat::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Option")
	float MinValue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Option")
	float MaxValue = 1.0f;

	/** Zero disables new rolls of this option. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Option", meta = (ClampMin = "0"))
	float Weight = 1.0f;

	bool Validate(FString& OutError) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};