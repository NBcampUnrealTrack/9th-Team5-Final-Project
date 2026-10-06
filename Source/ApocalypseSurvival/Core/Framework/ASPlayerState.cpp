#include "Core/Framework/ASPlayerState.h"

#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GAS/Attributes/ASCommonAttributeSet.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"

AASPlayerState::AASPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UASAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// Mixed: 본인 클라이언트는 효과 정보를 전부 받는다(예측과 UI 에 필요하다).
	// 다른 클라이언트는 태그·연출·어트리뷰트 값만 받는다.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	CommonAttributeSet = CreateDefaultSubobject<UASCommonAttributeSet>(TEXT("CommonAttributeSet"));
	CombatAttributeSet = CreateDefaultSubobject<UASCombatAttributeSet>(TEXT("CombatAttributeSet"));
	PlayerAttributeSet = CreateDefaultSubobject<UASPlayerAttributeSet>(TEXT("PlayerAttributeSet"));

	// PlayerState 는 기본 복제 주기가 느려서 그대로 두면 어트리뷰트 갱신이 늦게 보인다.
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* AASPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
