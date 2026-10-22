#pragma once

#include "NativeGameplayTags.h"

/** Native item tags shared by item data, UI, and gameplay abilities. */
namespace ASGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Consumable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Consumable_Healing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Consumable_Buff);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Consumable_Utility);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_Mana);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_Stamina);
	/** Maximum throw distance in cm, read by the use ability. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_ThrowRange);
	/** Explosion radius in cm, shared by the preview and grenade actor. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_ExplosionRadius);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_PlacementRange);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_BarricadeHealth);
	/** Seconds until removal; zero keeps the barricade until destroyed. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_BarricadeLifetime);
}

namespace ASItemMagnitudeDefaults
{
	inline constexpr float ThrowRange = 1000.0f;
	inline constexpr float ExplosionRadius = 300.0f;
	inline constexpr float PlacementRange = 300.0f;
	inline constexpr float BarricadeHealth = 300.0f;
	inline constexpr float BarricadeLifetime = 0.0f;
}
