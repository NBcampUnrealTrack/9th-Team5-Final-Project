#include "Core/UI/ViewModels/ASPlayerStatusViewModel.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/Texture2D.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "Characters/Player/Components/ASProgressionComponent.h"
#include "Core/Framework/ASPlayerState.h"
#include "GameFramework/Pawn.h"

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
	
	// 레벨, 경험치는 PlayerState의 성장 컴포넌트에 있다. 폰으로 들어오면 폰의 PlayerState를 찾는다
	const AASPlayerState* PlayerState = Cast<AASPlayerState>(InAbilitySystemOwner);
	if (!PlayerState)
	{
		if (const APawn* Pawn = Cast<APawn>(InAbilitySystemOwner))
		{
			PlayerState = Pawn->GetPlayerState<AASPlayerState>();
		}
	}
	
	BindProgression(PlayerState ? PlayerState->GetProgressionComponent() : nullptr);
}

void UASPlayerStatusViewModel::Deinitialize()
{
	if (ProgressionComponent.IsValid())
	{
		ProgressionComponent->OnLevelChanged.RemoveDynamic(this, &ThisClass::HandleLevelChanged);
		ProgressionComponent->OnExpChanged.RemoveDynamic(this, &ThisClass::HandleExpChanged);
	}
	ProgressionComponent.Reset();
	
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

void UASPlayerStatusViewModel::SetExperience(int32 InCurrentExp, int32 InExpToNextLevel)
{
	// 최대 레벨이면 다음 레벨 필요량이 0
	if (InExpToNextLevel <= 0)
	{
		UE_MVVM_SET_PROPERTY_VALUE(ExpPercent, 1.0f);
		UE_MVVM_SET_PROPERTY_VALUE(ExpText, NSLOCTEXT("PlayerStatusViewModel", "ExpMax", "Max"));;
		return;
	}
	
	const int32 SafeCurrentExp = FMath::Clamp(InCurrentExp, 0, InExpToNextLevel);
	UE_MVVM_SET_PROPERTY_VALUE(ExpPercent, static_cast<float>(SafeCurrentExp) / InExpToNextLevel);
	UE_MVVM_SET_PROPERTY_VALUE(ExpText, ASPlayerStatusViewModelPrivate::MakeResourceText(SafeCurrentExp, InExpToNextLevel));
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

void UASPlayerStatusViewModel::BindProgression(UASProgressionComponent* InProgressionComponent)
{
	if (!IsValid(InProgressionComponent))
	{
		return;
	}
	
	ProgressionComponent = InProgressionComponent;
	InProgressionComponent->OnLevelChanged.AddUniqueDynamic(this, &ThisClass::HandleLevelChanged);
	InProgressionComponent->OnExpChanged.AddUniqueDynamic(this, &ThisClass::HandleExpChanged);
	
	// 이벤트는 바뀔 때만 오므로 지금 값을 한 번 넣는다 (리스폰 후 HUD 재생성 때도 레벨 유지)
	SetPlayerLevel(InProgressionComponent->GetLevel());
	SetExperience(InProgressionComponent->GetCurrentExp(), InProgressionComponent->GetExpToNextLevel());
}

void UASPlayerStatusViewModel::HandleLevelChanged(int32 NewLevel)
{
	SetPlayerLevel(NewLevel);
	
	// 클라이언트는 레벨과 경험치가 따로 복제되므로, 레벨이 바뀌면 필요량도 다시 계산한다
	if (ProgressionComponent.IsValid())
	{
		SetExperience(ProgressionComponent->GetCurrentExp(), ProgressionComponent->GetExpToNextLevel());
	}
}

void UASPlayerStatusViewModel::HandleExpChanged(int32 CurrentExp, int32 ExpToNextLevel)
{
	SetExperience(CurrentExp, ExpToNextLevel);
}
