#pragma once

#include "CoreMinimal.h"
#include "ASItemTypes.generated.h"

/** High-level category used to decide how an item is stored and used. */
UENUM(BlueprintType)
enum class EASItemType : uint8
{
	Weapon UMETA(DisplayName = "Weapon"),
	Ammo UMETA(DisplayName = "Ammo"),
	Consumable UMETA(DisplayName = "Consumable")
};

/** Weapon behavior category. Only meaningful when ItemType is Weapon. */
UENUM(BlueprintType)
enum class EASWeaponType : uint8
{
	None UMETA(DisplayName = "None"),
	Melee UMETA(DisplayName = "Melee"),
	Ranged UMETA(DisplayName = "Ranged")
};

/** Resources checked before consuming a recovery item. */
UENUM(BlueprintType)
enum class EASItemRecoveryResource : uint8
{
	Health,
	Mana,
	Stamina
};
