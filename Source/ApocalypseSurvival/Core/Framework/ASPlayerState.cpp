#include "Core/Framework/ASPlayerState.h"

#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/ASAbilityTags.h"
#include "GAS/Abilities/ASGameplayAbility_CharacterPrimaryAttack.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "Items/ASItemInstance.h"
#include "Core/Input/ASInputTags.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"

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

	// ASC는 PlayerState에 남으므로, 이전 생명에서 남아 있던 무적 효과가 새 리스폰의
	// 보호 시간과 겹치지 않게 정리한다. GE_Invulnerable은 Effect.Invulnerable을 Asset Tag로 가져야 한다.
	FGameplayTagContainer InvulnerabilityEffectTags;
	InvulnerabilityEffectTags.AddTag(ASGameplayTags::Effect_Invulnerable);
	AbilitySystemComponent->RemoveActiveEffectsWithTags(InvulnerabilityEffectTags);

	// 기본값을 직접 바꾼다. 효과로 올리면 전투 중 걸린 버프·디버프와 섞이기 때문이다.
	AbilitySystemComponent->SetNumericAttributeBase(UASHealthAttributeSet::GetHealthAttribute(), HealthAttributeSet->GetMaxHealth());
	AbilitySystemComponent->SetNumericAttributeBase(UASPlayerAttributeSet::GetStaminaAttribute(), PlayerAttributeSet->GetMaxStamina());
	AbilitySystemComponent->SetNumericAttributeBase(UASPlayerAttributeSet::GetManaAttribute(), PlayerAttributeSet->GetMaxMana());
}

void AASPlayerState::SetCharacterDefinition(const UASCharacterDefinition* InDefinition)
{
	if (!HasAuthority() || bCharacterDefinitionApplied)
	{
		return;
	}
	
	CharacterDefinition = InDefinition;
}

void AASPlayerState::ApplyCharacterDefinitionOnce()
{
	if (!HasAuthority() || bCharacterDefinitionApplied)
	{
		return;
	}
	
	if (!IsValid(CharacterDefinition))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerState] 캐릭터 정의가 없어 어트리뷰트 기본값(C++)을 그대로 사용"));
		return;
	}
	
	// 성장 시스템이 생기기 전이라 레벨 1로 고정한다. 레벨은 이후 성장 컴포넌트가 관리한다
	constexpr int32 StartLevel = 1;
	
	// 두 바퀴 도는 이유: Health 같은 현재값은 MaxHealth 를 넘지 못하게 막혀 있다.
	// 목록 순서와 상관없이 첫 바퀴에 최대값이 들어가고, 두 번째 바퀴에 현재값이 제대로 들어간다.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const FASAttributeInitEntry& Entry : CharacterDefinition->BaseAttributes)
		{
			if (!Entry.Attribute.IsValid() || !AbilitySystemComponent->HasAttributeSetForAttribute(Entry.Attribute))
			{
				continue;
			}
			
			const float Value = CharacterDefinition->GetAttributeValueAtLevel(Entry.Attribute, StartLevel, Entry.Value);
			AbilitySystemComponent->SetNumericAttributeBase(Entry.Attribute, Value);
		}
	}
	
	bCharacterDefinitionApplied = true;
	UE_LOG(LogTemp, Log, TEXT("[PlayerState] 캐릭터 정의 적용: %s"), *GetNameSafe(CharacterDefinition));
}