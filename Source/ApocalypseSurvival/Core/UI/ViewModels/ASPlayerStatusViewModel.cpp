#include "Core/UI/ViewModels/ASPlayerStatusViewModel.h"

#include "Engine/Texture2D.h"

namespace ASPlayerStatusViewModelPrivate
{
	FText MakeResourceText(float CurrentValue, float MaxValue)
	{
		return FText::Format(
			NSLOCTEXT("PlayerStatusViewModel", "ResourceText", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(CurrentValue)),
			FText::AsNumber(FMath::RoundToInt(MaxValue)));
	}
}

void UASPlayerStatusViewModel::SetHealth(float InCurrentHealth, float InMaxHealth)
{
	const float SafeMaxHealth = FMath::Max(InMaxHealth, 0.0f);
	const float SafeCurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, SafeMaxHealth);
	const float NewPercent = SafeMaxHealth > 0.0f ? SafeCurrentHealth / SafeMaxHealth : 0.0f;

	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, NewPercent);
	UE_MVVM_SET_PROPERTY_VALUE(HealthText, ASPlayerStatusViewModelPrivate::MakeResourceText(SafeCurrentHealth, SafeMaxHealth));
}

void UASPlayerStatusViewModel::SetMana(float InCurrentMana, float InMaxMana)
{
	const float SafeMaxMana = FMath::Max(InMaxMana, 0.0f);
	const float SafeCurrentMana = FMath::Clamp(InCurrentMana, 0.0f, SafeMaxMana);
	const float NewPercent = SafeMaxMana > 0.0f ? SafeCurrentMana / SafeMaxMana : 0.0f;

	UE_MVVM_SET_PROPERTY_VALUE(ManaPercent, NewPercent);
	UE_MVVM_SET_PROPERTY_VALUE(ManaText, ASPlayerStatusViewModelPrivate::MakeResourceText(SafeCurrentMana, SafeMaxMana));
}

void UASPlayerStatusViewModel::SetPlayerLevel(int32 InPlayerLevel)
{
	UE_MVVM_SET_PROPERTY_VALUE(PlayerLevel, FMath::Max(InPlayerLevel, 1));
}

void UASPlayerStatusViewModel::SetPortraitTexture(UTexture2D* InPortraitTexture)
{
	UE_MVVM_SET_PROPERTY_VALUE(PortraitTexture, InPortraitTexture);
}
