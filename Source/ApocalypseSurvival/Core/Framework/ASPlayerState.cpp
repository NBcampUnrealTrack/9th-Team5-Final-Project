#include "Core/Framework/ASPlayerState.h"

#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/ASAbilityTags.h"
#include "GAS/Abilities/ASGameplayAbility_CharacterPrimaryAttack.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "Items/ASItemInstance.h"
#include "Core/Input/ASInputTags.h"

AASPlayerState::AASPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UASAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// Mixed: 본인 클라이언트는 효과 정보를 전부 받는다(예측과 UI 에 필요하다).
	// 다른 클라이언트는 태그·연출·어트리뷰트 값만 받는다.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthAttributeSet = CreateDefaultSubobject<UASHealthAttributeSet>(TEXT("HealthAttributeSet"));
	CombatAttributeSet = CreateDefaultSubobject<UASCombatAttributeSet>(TEXT("CombatAttributeSet"));
	PlayerAttributeSet = CreateDefaultSubobject<UASPlayerAttributeSet>(TEXT("PlayerAttributeSet"));

	// PlayerState 는 기본 복제 주기가 느려서 그대로 두면 어트리뷰트 갱신이 늦게 보인다.
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* AASPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AASPlayerState::GrantDefaultAbilitySetOnce(const UASAbilitySet* AbilitySet)
{
	if (!HasAuthority() || !IsValid(AbilitySet) || !DefaultAbilityHandles.IsEmpty())
	{
		return;
	}

	AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &DefaultAbilityHandles, this);

	// Migration fallback: once DA_AbilitySet_Player contains the native ability or a BP child,
	// this scan prevents a duplicate. It keeps existing content playable while equipment stops
	// granting/removing attacks on every weapon swap.
	bool bHasPrimaryAttack = false;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass()->IsChildOf(
			UASGameplayAbility_CharacterPrimaryAttack::StaticClass()))
		{
			bHasPrimaryAttack = true;
			break;
		}
	}
	if (!bHasPrimaryAttack)
	{
		FGameplayAbilitySpec AttackSpec(UASGameplayAbility_CharacterPrimaryAttack::StaticClass(), 1, INDEX_NONE, this);
		AttackSpec.GetDynamicSpecSourceTags().AddTag(ASGameplayTags::InputTag_Attack_Prepare);
		DefaultAbilityHandles.AddAbilitySpecHandle(AbilitySystemComponent->GiveAbility(AttackSpec));
	}
}

void AASPlayerState::SetEquippedItemRecord(UASItemInstance* Item)
{
	EquippedItemRecord = Item;
}

UASItemInstance* AASPlayerState::GetEquippedItemRecord() const
{
	return EquippedItemRecord.Get();
}

void AASPlayerState::ResetForRespawn()
{
	if (!HasAuthority())
	{
		return;
	}

	// 붙일 때와 같은 복제 설정으로 떼야 클라이언트에서도 사라진다.
	if (AbilitySystemComponent->IsDead())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(ASGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}

	// 기본값을 직접 바꾼다. 효과로 올리면 전투 중 걸린 버프·디버프와 섞이기 때문이다.
	AbilitySystemComponent->SetNumericAttributeBase(UASHealthAttributeSet::GetHealthAttribute(), HealthAttributeSet->GetMaxHealth());
	AbilitySystemComponent->SetNumericAttributeBase(UASPlayerAttributeSet::GetStaminaAttribute(), PlayerAttributeSet->GetMaxStamina());
	AbilitySystemComponent->SetNumericAttributeBase(UASPlayerAttributeSet::GetManaAttribute(), PlayerAttributeSet->GetMaxMana());
}
