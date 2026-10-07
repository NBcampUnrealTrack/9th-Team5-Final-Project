#include "Core/UI/ViewModels/ASSkillSlotViewModel.h"

#include "Engine/Texture2D.h"

void UASSkillSlotViewModel::InitializeSlot(const FText& InKeyBindText)
{
	UE_MVVM_SET_PROPERTY_VALUE(KeyBindText, InKeyBindText);
}

void UASSkillSlotViewModel::SetSkillIcon(UTexture2D* InSkillIcon)
{
	UE_MVVM_SET_PROPERTY_VALUE(SkillIcon, InSkillIcon);
}

void UASSkillSlotViewModel::SetCooldownPercent(float InCooldownPercent)
{
	UE_MVVM_SET_PROPERTY_VALUE(CooldownPercent, FMath::Clamp(InCooldownPercent, 0.0f, 1.0f));
}

void UASSkillSlotViewModel::SetStackCount(int32 InStackCount)
{
	UE_MVVM_SET_PROPERTY_VALUE(StackCount, FMath::Max(InStackCount, 0));
}

void UASSkillSlotViewModel::SetEnabled(bool bInIsEnabled)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsEnabled, bInIsEnabled);
}

void UASSkillSlotViewModel::SetOnCooldown(bool bInIsOnCooldown)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsOnCooldown, bInIsOnCooldown);
}

void UASSkillSlotViewModel::SetHasSkill(bool bInHasSkill)
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasSkill, bInHasSkill);
}
