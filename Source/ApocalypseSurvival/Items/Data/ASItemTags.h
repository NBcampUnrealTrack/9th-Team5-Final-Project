#pragma once

#include "NativeGameplayTags.h"

/** Native item tags shared by item data, UI, and gameplay abilities. */
namespace ASGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Consumable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Consumable_Healing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Consumable_Buff);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Consumable_Utility);
}
