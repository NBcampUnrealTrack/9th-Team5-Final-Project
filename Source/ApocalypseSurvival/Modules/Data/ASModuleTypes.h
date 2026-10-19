#pragma once

#include "CoreMinimal.h"
#include "ASModuleTypes.generated.h"

UENUM(BlueprintType)
enum class EASModuleCategory : uint8
{
	Attack,
	Skill,
	Survival,
	Utility
};

/** Additive equipment bonuses supported by the existing character attribute sets. */
UENUM(BlueprintType)
enum class EASModuleStat : uint8
{
	None,
	AttackPower,
	MaxHealth,
	MaxMana,
	MaxStamina,
	MoveSpeed
};

/** One rolled option on an owned module. The value is saved so balance edits never reroll loot. */
USTRUCT(BlueprintType)
struct FASModuleOptionRoll
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly)
	FName OptionId;

	/** Saved together with Value so sheet changes cannot alter owned loot. */
	UPROPERTY(SaveGame, BlueprintReadOnly)
	EASModuleStat Stat = EASModuleStat::None;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	float Value = 0.0f;

	bool operator==(const FASModuleOptionRoll& Other) const { return OptionId == Other.OptionId && Stat == Other.Stat && Value == Other.Value; }
};

/** An owned, individually rolled module. */
USTRUCT(BlueprintType)
struct FASModuleInstanceData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly)
	FGuid InstanceId;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	FName ModuleId;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	TArray<FASModuleOptionRoll> Options;

	bool IsEmpty() const { return !InstanceId.IsValid() && ModuleId.IsNone() && Options.IsEmpty(); }
	bool operator==(const FASModuleInstanceData& Other) const { return InstanceId == Other.InstanceId && ModuleId == Other.ModuleId && Options == Other.Options; }
};
