#include "GAS/Attributes/ASCombatAttributeSet.h"

#include "Net/UnrealNetwork.h"

UASCombatAttributeSet::UASCombatAttributeSet()
{
	// 캐릭터별 데이터가 생기기 전까지 쓰는 임시 기본값이다.
	InitAttackPower(10.0f);
	InitMoveSpeed(600.0f);
}

void UASCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UASCombatAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UASCombatAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
}

void UASCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 둔화 효과가 겹쳐도 뒤로 걷는 일은 없어야 한다.
	if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UASCombatAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UASCombatAttributeSet, AttackPower, OldValue);
}

void UASCombatAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UASCombatAttributeSet, MoveSpeed, OldValue);
}
