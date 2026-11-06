#pragma once
#include "CoreMinimal.h"

class UBorder;
class UButton;
class UTextBlock;
class UWidgetTree;

/** Shared, flat field-equipment palette. Rarity colors remain reserved for actual items. */
namespace ASMenuStyle
{
	inline const FLinearColor Background(.024f, .027f, .025f, .99f);
	inline const FLinearColor Surface(.043f, .047f, .043f, 1.f);
	inline const FLinearColor Line(.13f, .14f, .12f, 1.f);
	inline const FLinearColor Text(.83f, .82f, .75f, 1.f);
	inline const FLinearColor Muted(.43f, .46f, .42f, 1.f);
	inline const FLinearColor Accent(.66f, .53f, .28f, 1.f);
	inline const FLinearColor Success(.43f, .56f, .38f, 1.f);
	inline const FLinearColor Danger(.68f, .33f, .26f, 1.f);
	APOCALYPSESURVIVAL_API void StyleText(UTextBlock* TextBlock, int32 Size = 14, FLinearColor Color = Text, bool bBold = false);
	APOCALYPSESURVIVAL_API void StyleButton(UButton* Button, bool bPrimary = false);
	APOCALYPSESURVIVAL_API void StyleAuthoredTree(UWidgetTree* Tree);
	APOCALYPSESURVIVAL_API void StyleWindow(UBorder* Frame);
}
