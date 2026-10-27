#include "GAS/Attributes/ASHealthAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "GAS/ASAbilityTags.h"
#include "Net/UnrealNetwork.h"

namespace
{
	//피해 감소율 상한 (완전 무적 방지)
	constexpr float MaxDamageReduction = 0.9f;
}

UASHealthAttributeSet::UASHealthAttributeSet()
{
	// 캐릭터별 데이터가 생기기 전까지 쓰는 임시 기본값이다.
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitDamageReduction(0.0f);
}

void UASHealthAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// COND_None + REPNOTIFY_Always 는 GAS 의 표준 설정이다.
	// 값이 같아도 알림을 받아야 예측이 빗나갔을 때 되돌릴 수 있다.
	DOREPLIFETIME_CONDITION_NOTIFY(UASHealthAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UASHealthAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UASHealthAttributeSet, DamageReduction, COND_None, REPNOTIFY_Always);
}

void UASHealthAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetDamageReductionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, MaxDamageReduction);
	}
}

bool UASHealthAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	//피해(Health 감소)일 때 DamageReduction만큼 줄임 (데드 앵글 등)
	if (Data.EvaluatedData.Attribute == GetHealthAttribute() && Data.EvaluatedData.Magnitude < 0.0f)
	{
		// BP가 피해 효과를 직접 적용해도 무적 태그가 있으면 체력을 깎지 않는다.
		if (Data.Target.HasMatchingGameplayTag(ASGameplayTags::State_Invulnerable))
		{
			return false;
		}

		const float Reduction = FMath::Clamp(GetDamageReduction(), 0.0f, MaxDamageReduction);
		Data.EvaluatedData.Magnitude *= (1.0f - Reduction);
	}

	return true;
}

void UASHealthAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// PreAttributeChange 는 현재값만 막는다. 즉발 효과는 기본값을 직접 바꾸므로
	// 여기서 기본값도 범위 안으로 맞춰야 한다.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}

void UASHealthAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UASHealthAttributeSet, Health, OldValue);
}

void UASHealthAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UASHealthAttributeSet, MaxHealth, OldValue);
}

void UASHealthAttributeSet::OnRep_DamageReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UASHealthAttributeSet, DamageReduction, OldValue);
}
