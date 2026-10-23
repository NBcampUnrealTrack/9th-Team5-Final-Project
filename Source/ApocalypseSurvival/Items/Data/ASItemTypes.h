#pragma once

#include "CoreMinimal.h"
#include "ASItemTypes.generated.h"

/** Shared quality tiers for item definitions and loot chests. */
UENUM(BlueprintType)
enum class EASLootRarity : uint8
{
	Uncommon,
	Rare,
	Epic,
	Legendary
};

constexpr bool IsValidLootRarity(const EASLootRarity Rarity)
{
	return Rarity >= EASLootRarity::Uncommon && Rarity <= EASLootRarity::Legendary;
}

/** Shared visual accent: 90% sRGB base color and 10% white, converted to linear. */
inline FLinearColor GetLootRarityColor(const EASLootRarity Rarity)
{
	switch (Rarity)
	{
	case EASLootRarity::Rare: return FLinearColor(FColor(26, 26, 255));
	case EASLootRarity::Epic: return FLinearColor(FColor(141, 26, 255));
	case EASLootRarity::Legendary: return FLinearColor(FColor(255, 26, 26));
	default: return FLinearColor::White;
	}
}

/** High-level category used to decide how an item is stored and used. */
UENUM(BlueprintType)
enum class EASItemType : uint8
{
	Weapon UMETA(DisplayName = "Weapon"),
	Ammo UMETA(DisplayName = "Ammo"),
	Consumable UMETA(DisplayName = "Consumable"),
	/** Stackable crafting ingredient; it is neither equipped nor used directly. */
	CraftingMaterial UMETA(DisplayName = "Crafting Material")
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
