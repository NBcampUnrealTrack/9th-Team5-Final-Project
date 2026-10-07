#include "Core/UI/ViewModels/ASPlayerStatusViewModel.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/Texture2D.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"

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

void UASPlayerStatusViewModel::Initialize(UAbilitySystemComponent* InAbilitySystemComponent)
{
	Deinitialize();

	if (!IsValid(InAbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	HealthChangedHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged);
	MaxHealthChangedHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::HandleMaxHealthChanged);
	ManaChangedHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASPlayerAttributeSet::GetManaAttribute())
		.AddUObject(this, &ThisClass::HandleManaChanged);
	MaxManaChangedHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASPlayerAttributeSet::GetMaxManaAttribute())
		.AddUObject(this, &ThisClass::HandleMaxManaChanged);

	// 델리게이트는 값이 바뀔 때만 실행되므로, 구독 직후 현재값을 별도로 반영한다.
	RefreshHealth();
	RefreshMana();
}

void UASPlayerStatusViewModel::InitializeFromActor(AActor* InAbilitySystemOwner)
{
	Initialize(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InAbilitySystemOwner));
}

void UASPlayerStatusViewModel::Deinitialize()
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetMaxHealthAttribute())
			.Remove(MaxHealthChangedHandle);
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UASPlayerAttributeSet::GetManaAttribute())
			.Remove(ManaChangedHandle);
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UASPlayerAttributeSet::GetMaxManaAttribute())
			.Remove(MaxManaChangedHandle);
	}

	AbilitySystemComponent.Reset();
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	ManaChangedHandle.Reset();
	MaxManaChangedHandle.Reset();
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

void UASPlayerStatusViewModel::HandleHealthChanged(const FOnAttributeChangeData&)
{
	RefreshHealth();
}

void UASPlayerStatusViewModel::HandleMaxHealthChanged(const FOnAttributeChangeData&)
{
	RefreshHealth();
}

void UASPlayerStatusViewModel::HandleManaChanged(const FOnAttributeChangeData&)
{
	RefreshMana();
}

void UASPlayerStatusViewModel::HandleMaxManaChanged(const FOnAttributeChangeData&)
{
	RefreshMana();
}

void UASPlayerStatusViewModel::RefreshHealth()
{
	if (!AbilitySystemComponent.IsValid())
	{
		return;
	}

	SetHealth(
		AbilitySystemComponent->GetNumericAttribute(UASHealthAttributeSet::GetHealthAttribute()),
		AbilitySystemComponent->GetNumericAttribute(UASHealthAttributeSet::GetMaxHealthAttribute()));
}

void UASPlayerStatusViewModel::RefreshMana()
{
	if (!AbilitySystemComponent.IsValid())
	{
		return;
	}

	SetMana(
		AbilitySystemComponent->GetNumericAttribute(UASPlayerAttributeSet::GetManaAttribute()),
		AbilitySystemComponent->GetNumericAttribute(UASPlayerAttributeSet::GetMaxManaAttribute()));
}
