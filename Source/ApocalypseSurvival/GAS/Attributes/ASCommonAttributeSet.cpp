#include "GAS/Attributes/ASCommonAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UASCommonAttributeSet::UASCommonAttributeSet()
{
	// 캐릭터별 데이터가 생기기 전까지 쓰는 임시 기본값이다.
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
}

void UASCommonAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// COND_None + REPNOTIFY_Always 는 GAS 의 표준 설정이다.
	// 값이 같아도 알림을 받아야 예측이 빗나갔을 때 되돌릴 수 있다.
	DOREPLIFETIME_CONDITION_NOTIFY(UASCommonAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UASCommonAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UASCommonAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
}

void UASCommonAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// PreAttributeChange 는 현재값만 막는다. 즉발 효과는 기본값을 직접 바꾸므로
	// 여기서 기본값도 범위 안으로 맞춰야 한다.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}

void UASCommonAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UASCommonAttributeSet, Health, OldValue);
}

void UASCommonAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UASCommonAttributeSet, MaxHealth, OldValue);
}
