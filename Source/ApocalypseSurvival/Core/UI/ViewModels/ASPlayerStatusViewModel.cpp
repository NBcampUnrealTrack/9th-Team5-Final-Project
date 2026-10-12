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
	StaminaChangedHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASPlayerAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleStaminaChanged);
	MaxStaminaChangedHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASPlayerAttributeSet::GetMaxStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleMaxStaminaChanged);

	// 델리게이트는 값이 바뀔 때만 실행되므로, 구독 직후 현재값을 별도로 반영한다.
	RefreshHealth();
	RefreshMana();
	RefreshStamina();
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
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UASPlayerAttributeSet::GetStaminaAttribute())
			.Remove(StaminaChangedHandle);
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UASPlayerAttributeSet::GetMaxStaminaAttribute())
			.Remove(MaxStaminaChangedHandle);
	}

	AbilitySystemComponent.Reset();
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	ManaChangedHandle.Reset();
	MaxManaChangedHandle.Reset();
	StaminaChangedHandle.Reset();
	MaxStaminaChangedHandle.Reset();
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

void UASPlayerStatusViewModel::SetStamina(float InCurrentStamina, float InMaxStamina)
{
	const float SafeMaxStamina = FMath::Max(InMaxStamina, 0.0f);
	const float SafeCurrentStamina = FMath::Clamp(InCurrentStamina, 0.0f, SafeMaxStamina);
	const float NewPercent = SafeMaxStamina > 0.0f ? SafeCurrentStamina / SafeMaxStamina : 0.0f;

	UE_MVVM_SET_PROPERTY_VALUE(StaminaPercent, NewPercent);
	UE_MVVM_SET_PROPERTY_VALUE(StaminaText, ASPlayerStatusViewModelPrivate::MakeResourceText(SafeCurrentStamina, SafeMaxStamina));
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

void UASPlayerStatusViewModel::HandleStaminaChanged(const FOnAttributeChangeData&)
{
	RefreshStamina();
}

void UASPlayerStatusViewModel::HandleMaxStaminaChanged(const FOnAttributeChangeData&)
{
	RefreshStamina();
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

void UASPlayerStatusViewModel::RefreshStamina()
{
	if (!AbilitySystemComponent.IsValid())
	{
		return;
	}

	SetStamina(
		AbilitySystemComponent->GetNumericAttribute(UASPlayerAttributeSet::GetStaminaAttribute()),
		AbilitySystemComponent->GetNumericAttribute(UASPlayerAttributeSet::GetMaxStaminaAttribute()));
}
